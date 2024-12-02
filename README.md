# Tema1a APD - Stan Andrei Razvan 333CA

## Enunt
In cadrul temei am avut de implementat folosind Pthreads din C/C++ un program paralel care sa gaseasca indexul inversat pentru o multime de fisiere de intrare. 

Codul trebuia scris avand in vedere paradigma Mapper-Reducer. Din fisierele de intrare trebuiesc extrase cuvinte si in final trebuie sa rezulte fisiere de tipul `litera.txt`, unde se vor afla cuvintele care incep cu o litera si indexul fisierelor unde se regasesc aceste cuvinte.

## Implementare
Am ales sa implementez tema in C++ pentru a putea folosi structurile de date deja definite aici (map, vector, etc) dar si functia `sort`.

### Gandire generala
In enunt ni se cere ca thread-urile, atat mapper cat si reducer sa fie pornite in acelasi for (valabil si pentru join). Pentru a realiza acest lucru m-am folosit de o bariera initializata cu totalul de threaduri. In momentul in care un mapper termina acesta va "astepta la bariera" si pentru celelalte. De asemenea, thread-urile reducer la inceput asteapta la aceeasi bariera sa termine toti mapperii.

Pentru o mai buna scalabilitate si distributie a datelor procesate fiecarte functie de thread-uri (mapper si reducer) incepe cu un `while (true)`. Fiecare thread va lua ceva de procesat, cand termina se va intoarce si se va uita daca mai poate procesa ceva, daca DA o face si daca NU, se da break si acel thread si-a terminat treaba.

Structura de date `thread_data` contine toate datele de care au nevoie threadurile, pentru a evita variabilele globale. Aici am adaugat campuri pentru id-ul thread-ului curent, mutexii folositi, bariera, dar si `result_map` care este un vector de perechi tip `<string, int>` (cuvant, index fisier) si este rezultatul thread-urilor map.

Am inclus sortarea rezultatului final si crearea de fisiere in functia de reduce.

### Distributia echitabila a task-urilor
Primul attempt la tema a fost facut o distributie ca cea din laboratorul 1 de APD a task-urilor, folosind calcularea unor indici `start` si `end`, dar am observat ca asta nu ne da o scalabilitate foarte buna si nici nu este foarte eficient.

Pentru a putea realiza distributia datelor care trebuiesc procesate intre threadurile de acelasi tip m-am gandit ce face fiecare tip de thread.

Mapperii iau cuvinte din fisiere, deci fiecare mapper proceseaza un fisier. Reducerii "reduc" vectorul `result_map`. Practic reducerii se uita in rezultatul de la map si daca gasesc un cuvant care se regaseste in mai multe fisiere vor adauga in set-ul de int-uri pentru cuvantul respectiv toate indexurile la care se regaseste.

Avand aceste lucruri in vedere am decis sa fac doi vectori de tip `vector<bool>`, anume : `files_finished` si `letters_finished`. Valoarea false inseamna ca un fisier/ o litera nu a fost inca procesata. Fiecare mapper se uita daca mai sunt fisiere pe false si le proceseaza, iar fiecare reducer se uita daca mai sunt litere neprocesate si le proceseaza. Am decis ca reducerii vor alege ce cuvinte va procesa in functie de litera cu care incep pentru ca ele trebuie sa creeze si fisierele, acest lucru facandu-se in functie de litera.

Aceasta distributie va fi facuta intr-o zona critica pentru a asigura ca doua threaduri nu vor alege aceleasi lucuri (nu vor marca cu true).

Fiecare thread (fie el mapper sau reducer) isi va crea un vector/map local pentru a procesa datele, urmand ca atunci cand a terminat sa uneasca datele intr-o zona critica (in cazul map, pentru reducer doar se creeaza fisierul).

### Observatie

Am folosit bibilioteca `pthread_barrier.h` din laborator deoarece am lucrat pe Mac unde aceasta biblioteca nu este implementata.


