#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <vector>
#include <pthread.h>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <algorithm>
#include "pthread_barrier.h"

using namespace std;

typedef struct {
    //id thread
    int thread_id;
    //numele fisierelor
    vector<string> files;
    //vector cu fisierele procesate, accesibil pt toate threadurile
    vector<bool> *files_finished;
    //vector cu literele procesarte de reducer
    vector<bool> *letters_finished;
    //mutexes
    pthread_mutex_t *mutex;
    pthread_mutex_t *mutex_files;
    //bariera pentru a sti cand threadurile map termina treaba
    pthread_barrier_t *barrier;
    //rezultatul threadurilor map
    //map<string, set<int>> *result_map;
    vector<pair<string, int>> *result_map;
} thread_data;

bool isSeparator(char c) {
    return !isalpha(c);
}

thread_data *init_data(int thread_id, vector<string> files, vector<bool> *files_finished, vector<bool> *letters_finished, 
                        pthread_mutex_t *mutex, pthread_mutex_t *mutex_files, pthread_barrier_t *barrier, 
                        vector<pair<string, int>> *result_map) {
    thread_data *data = (thread_data *)malloc(sizeof(thread_data));
    data->thread_id = thread_id;
    data->files = files;
    data->files_finished = files_finished;
    data->letters_finished = letters_finished;
    data->mutex = mutex;
    data->mutex_files = mutex_files;
    data->barrier = barrier;
    data->result_map = result_map;
    return data;
}

void *mapper(void *arg) {
    thread_data *data = (thread_data *)arg;
    vector<pair<string, int>> local_result_map;
    while (true) {
        int file_index = -1;
        bool ok = false;
        for (int i = 0; i < (int)(data->files.size()); i++) {
            //daca mai avem fisiere nefacute il preia threadul liber si il marcheaza cu true in files_finished
            pthread_mutex_lock(data->mutex_files);
            if (!(*data->files_finished)[i]) {
                (*data->files_finished)[i] = true;
                pthread_mutex_unlock(data->mutex_files);
                file_index = i;
                ok = true;
                break;
            }
            //in caz ca nu se intra pe if
            pthread_mutex_unlock(data->mutex_files);
        }
        if (ok == false)
            break;
        ifstream file(data->files[file_index]);
        if (!file.is_open()) {
            cerr << "Could not open file: " << data->files[file_index] << endl;
            continue;
        }
        string word;
        while (file >> word) {
            string cleanWord = "";
            for (int i = 0; i < word.size(); i++) {
                if (!isSeparator(word[i])) 
                    cleanWord += tolower(word[i]);
            }
            //daca cuvantul este gol, nu il procesam
            if (cleanWord.size() == 0)
                continue;
            //daca cuvantul nu exista in map, il adaugam
            pair<string, int> entry(cleanWord, file_index + 1);
            local_result_map.push_back(entry);
        }
        file.close();
    }
    //se adauga rezultatul local in rezultatul global
    pthread_mutex_lock(data->mutex);
    for (const auto &entry : local_result_map) {
        data->result_map->push_back(entry);
    }
    pthread_mutex_unlock(data->mutex);
    //threadul a terminat si asteapta dupa ceilalti
    pthread_barrier_wait(data->barrier);
    free(data);
    pthread_exit(NULL);
}

void create_file(int letter_index, vector<pair<string, set<int>>> &sorted_local_result_reduce) {
    string filename = string(1, 'a' + letter_index) + ".txt";
    ofstream outFile(filename);
    for (const auto &wordEntry : sorted_local_result_reduce) {
        outFile << wordEntry.first << ":[";
        for (auto it = wordEntry.second.begin(); it != wordEntry.second.end(); ++it) {
            if (it != wordEntry.second.begin()) 
                outFile << " ";
            outFile << *it;
        }
        outFile << "]" << endl;
    }
    outFile.close();
}

void *reducer(void *arg) {
    thread_data *data = (thread_data *)arg;
    //se asteapta dupa map-uri
    pthread_barrier_wait(data->barrier);
    while (true) {
        int letter_index = -1;
        bool ok = false;
        for (int i = 0; i < 26; i++) {
            //daca mai avem litere neprocesate, threadul curent o va procesa
            pthread_mutex_lock(data->mutex);
            if (!(*data->letters_finished)[i]) {
                (*data->letters_finished)[i] = true;
                pthread_mutex_unlock(data->mutex);
                letter_index = i;
                ok = true;
                break;
            }
            pthread_mutex_unlock(data->mutex);
        }
        //daca nu mai avem litere ne oprim
        if (ok == false)
            break;
        //map cu cuvantul si setul de fisiere in care se regaseste
        map<string, set<int> > local_result_reduce;
        for (const auto &entry : *data->result_map) {
            //cautam toate cuvintele care incep cu litera pe care o procesam
            if (tolower(entry.first[0]) == 'a' + letter_index) {
                if (local_result_reduce.find(entry.first) == local_result_reduce.end()) {
                    set<int> indexes;
                    indexes.insert(entry.second);
                    local_result_reduce[entry.first] = indexes;
                } else {
                    //daca avme deja cuvantul ii adaugam inca un id
                    local_result_reduce[entry.first].insert(entry.second);
                }
            }
        }
        vector<pair<string, set<int> > > sorted_local_result_reduce(local_result_reduce.begin(), local_result_reduce.end());
        sort(sorted_local_result_reduce.begin(), sorted_local_result_reduce.end(), [](const pair<string, set<int> > &a, const pair<string, set<int> > &b) {
            if (a.second.size() == b.second.size()) {
                return a.first < b.first;
            }
            return a.second.size() > b.second.size();
        });
        //se fac fisierele, fiecare thread va face fisier pentru liteera pe care o proceseaza la momentul ala
        create_file(letter_index, sorted_local_result_reduce);
    }
    free(data);
    pthread_exit(NULL);
}

int main(int argc, char **argv) {
    if (argc != 4) {
        cout << "Usage: ./tema1 <numar_mapperi> <numar_reduceri> <fisier_intrare>" << endl;
        return -1;
    }

    int numar_mapperi = atoi(argv[1]);
    int numar_reduceri = atoi(argv[2]);
    string fisier_intrare = argv[3];

    ifstream inputFile(fisier_intrare);
    if (!inputFile.is_open()) {
        cerr << "Could not open input file: " << fisier_intrare << endl;
        return -1;
    }

    string line;
    vector<string> lines;

    while (getline(inputFile, line))
        lines.push_back(line);
    inputFile.close();

    int number_of_files = stoi(lines[0]);
    lines.erase(lines.begin());

    vector<string> files(lines.begin(), lines.begin() + number_of_files);
    vector<bool> files_finished(number_of_files, false);
    vector<bool> letters_finished(26, false);
    vector<pair<string, int>> result_map;

    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);

    pthread_mutex_t mutex_files;
    pthread_mutex_init(&mutex_files, NULL);

    //se vor astepta mapperii folosind bariera, de aasta este initializata cu numar_mapperi + numar_reduceri
    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, numar_mapperi + numar_reduceri);

    pthread_t mappers[numar_mapperi];
    pthread_t reducers[numar_reduceri];
    for (int i = 0; i < numar_mapperi + numar_reduceri; i++) {
        if (i < numar_mapperi) {
            thread_data *data = init_data(i, files, &files_finished, &letters_finished, &mutex, &mutex_files, &barrier, &result_map);
            pthread_create(&mappers[i], NULL, mapper, data);
        }
        if (i >= numar_mapperi) {
            thread_data *data = init_data(i, files, &files_finished, &letters_finished, &mutex, &mutex_files, &barrier, &result_map);
            pthread_create(&reducers[i - numar_mapperi], NULL, reducer, data);
        }
    }

    for (int i = 0; i < numar_mapperi + numar_reduceri; i++) {
        if (i < numar_mapperi) {
            pthread_join(mappers[i], NULL);
        }
        if (i >= numar_mapperi) {
            pthread_join(reducers[i - numar_mapperi], NULL);
        }
    }

    pthread_mutex_destroy(&mutex);
    pthread_mutex_destroy(&mutex_files);
    pthread_barrier_destroy(&barrier);

    return 0;
}