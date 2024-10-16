#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <ctype.h>

extern int total_guesses;
extern int total_wins;
extern int total_losses;
extern char** words;

char ** dictionary;
int dict_size;
int words_size;
int listener;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// Check if string is a num
int is_int(char* arg) {
    size_t i;

    for (i = 0; i < strlen(arg); ++i) {
        if (!isdigit(*(arg + i))) {
            return 0;
        }
    }

    return 1;
}

// Validate the word in dictionary
int valid_word(char* str) {
    size_t i;

    for (i = 0; i < strlen(str); ++i) {
        if (isspace(*(str + i)) || isdigit(*(str + i)))
            return 0;
    }

    return 1;
}

int valid_dict_file(int dict_file, int num_dictionary) {
    dictionary = (char**) calloc(1, sizeof(char*));
    int total_dictionary = 0;
    char* buffer = (char*) calloc(6, sizeof(char));
    char tmp;
    int rc = read(dict_file, &tmp, 1);

    while (rc > 0) {
        if (rc == -1) {
            perror("read() failed!");
            free(buffer);
            return 0;
        }
        
        if (!isspace(tmp)) {
            *(buffer) = tmp;
            rc = read(dict_file, buffer + 1, 4);

            if (rc == -1) {
                perror("read() failed!");
                free(buffer);
                return 0;
            }
            
            if (!valid_word(buffer)) {
                free(buffer);
                return 0;
            }

            else {
                if (rc == 0) {
                    break;
                }

                if (rc > 0 && rc < 4) {
                    free(buffer);
                    return 0;
                }

                if (total_dictionary != 0) {
                    dictionary = (char**) realloc(dictionary, (total_dictionary + 1) * sizeof(char*));
                }

                *(dictionary + total_dictionary) = (char*) calloc(6, sizeof(char));
                strcpy(*(dictionary + total_dictionary), buffer);
                total_dictionary++;
            }
        }

        else {
            if (tmp == '\r') {
                rc = read(dict_file, &tmp ,1);
            }

            rc = read(dict_file, buffer, 5);

            if (rc == -1) {
                perror("read() failed!");
                free(buffer);
                return 0;
            }
            
            if (!valid_word(buffer)) {
                free(buffer);
                return 0;
            }

            else {
                if (rc == 0) {
                    break;
                }

                if (rc > 0 && rc < 5) {
                    free(buffer);
                    return 0;
                }

                if (total_dictionary != 0) {
                    dictionary = (char**) realloc(dictionary, (total_dictionary + 1) * sizeof(char*));
                }

                *(dictionary + total_dictionary) = (char*) calloc(6, sizeof(char));
                strcpy(*(dictionary + total_dictionary), buffer);
                total_dictionary++;
            }
        }

        rc = read(dict_file, &tmp, 1);
    }

    free(buffer);

    if (total_dictionary != num_dictionary) {
        return 0;
    }

    return 1;
}

void free_dictionary() {
    int i;

    for (i = 0; i < dict_size; ++i) {
        free(*(dictionary + i));
    }

    free(dictionary);
}

int same_word(char* word1, char* word2) {
    if (strlen(word1) != strlen(word2)) {
        return 0;
    }

    size_t i;

    for (i = 0; i < strlen(word1); ++i) {
        char w1 = tolower(*(word1 + i));
        char w2 = tolower(*(word2 + i));
        if (w1 != w2) {
            return 0;
        }
    }

    return 1;
}

char valid_guess(char* word, int num_dictionary) {
    if (strlen(word) != 5) {
        return 'N';
    }

    int i;
    int is_valid = 'N';

    for (i = 0; i < num_dictionary; ++i) {
        if (same_word(*(dictionary + i), word) != 0) {
            is_valid = 'Y';
        }
    }

    return is_valid;
}

char* select_word() {
    int index = rand() % dict_size;
    return *(dictionary + index);
}

char get_letter(char* hidden_word, char* guess, int curr_index) {
    int i;
    int* in_hidden = (int*) calloc(1, sizeof(int));
    int in_hidden_size = 0;
    int* in_guess = (int*) calloc(1, sizeof(int));
    int in_guess_size = 0;
    char letter_guess = tolower(*(guess + curr_index));
    char letter = '-';

    for (i = 0; i < 5; ++i) {
        char curr_char_in_hidden = tolower(*(hidden_word + i));
        char curr_char_in_guess = tolower(*(guess + i));
        
        if (curr_char_in_hidden == letter_guess) {
            *(in_hidden + in_hidden_size) = i;
            in_hidden = (int*) realloc(in_hidden, (in_hidden_size + 1) * sizeof(int));
            in_hidden_size++;
        }

        if (curr_char_in_guess == letter_guess) {
            *(in_guess + in_guess_size) = i;
            in_guess = (int*) realloc(in_guess, (in_guess_size + 1) * sizeof(int));
            in_guess_size++;
        }
    }

    if (in_guess_size <= in_hidden_size) {
        letter = letter_guess;
        char curr_char_in_hidden = tolower(*(hidden_word + curr_index));
        char curr_char_in_guess = tolower(*(guess + curr_index));

        if (curr_char_in_hidden == curr_char_in_guess) {
            letter = toupper(letter_guess);
        }
    }

    else if (in_guess_size > in_hidden_size && in_hidden_size > 0) {
        int count_g = 0;

        int intersect_before = 0;
        int intersect_after = 0;
        int intersect_at = 0;

        for (i = 0; i < 5; ++i) {
            char curr_char_in_hidden = tolower(*(hidden_word + i));
            char curr_char_in_guess = tolower(*(guess + i));

            if (curr_char_in_guess == letter_guess) {
                if (i <= curr_index) {
                    count_g++;
                }
            }

            if (curr_char_in_guess == curr_char_in_hidden) {
                if (curr_char_in_guess == letter_guess) {
                    if (i < curr_index) {
                        intersect_before = 1;
                    }

                    else if (i > curr_index) {
                        intersect_after = 1;
                    }

                    else {
                        intersect_at = 1;
                    }
                }
            }
        }

        if (intersect_after == 0 && intersect_before == 0 && intersect_at == 0) {
            if (count_g <= in_hidden_size) {
                letter = letter_guess;
            }
        }

        if (intersect_after > 0 || intersect_before > 0) {
            if (count_g < in_hidden_size) {
                letter = letter_guess;
            }
        }

        if (intersect_at > 0) {
            letter = toupper(letter_guess);
        }
    }

    free(in_hidden);
    free(in_guess);
    return letter;
}

char* guess_reply(char* hidden_word, char* guess) {
    char* reply = (char*) calloc(6, sizeof(char));
    int i;

    for (i = 0; i < 5; ++i) {
        *(reply + i) = get_letter(hidden_word, guess, i);
    }

    return reply;
}

void handle_signal(int sig) {
    if (sig == SIGUSR1) {
        printf("MAIN: SIGUSR1 rcvd; Wordle server shutting down...\n");
        printf("MAIN:MAIN: valid guesses: %d\n", total_guesses);
        printf("MAIN:MAIN: win/loss: %d/%d\n", total_wins, total_losses);

        int i;
        for (i = 0; i < words_size; ++i) {
            printf("MAIN:MAIN: word #%d: %s\n", i + 1, *(words + i));
        }

        close(listener);
        free_dictionary();
        exit(EXIT_SUCCESS);
    }
}

void capitalize_word(char* word) {
    int i;

    for (i = 0; i < 5; ++i) {
        *(word + i) = toupper(*(word + i));
    }
}

// Function to handle each client connection
void* handle_client(int* newsd) {
    int win = 0;
    int client = *(newsd);
    char* guess = calloc(6, sizeof(char));
    char* hidden_word = select_word();
    capitalize_word(hidden_word);
    words = (char**) realloc(words, (words_size + 1) * sizeof(char*));
    *(words + words_size) = (char*) calloc(6, sizeof(char));
    strcpy(*(words + words_size), hidden_word);
    pthread_mutex_lock(&mutex);
    words_size++;
    pthread_mutex_unlock(&mutex);
    int num_guesses = 6;
    printf("MAIN: rcvd incoming connection request\n");
    printf("THREAD %lu: waiting for guess\n", (unsigned long)pthread_self());
    int n = recv(client, guess, 5, MSG_WAITALL);

    if (n == 0) {
        printf("THREAD %lu: client gave up; closing TCP connection...\n", (unsigned long)pthread_self());
    }

    else if (n < 0) {
        perror("ERROR: recv() failed!");
    }

    while (num_guesses > 0 && n > 0) {
        printf("THREAD %lu: rcvd guess: %s\n", (unsigned long)pthread_self(), guess);
        char* reply = (char*) calloc(9, sizeof(char));

        if (valid_guess(guess, dict_size) == 'Y') {
            num_guesses--;
            pthread_mutex_lock(&mutex);
            total_guesses++;
            pthread_mutex_unlock(&mutex);
            *(reply) = 'Y';
            char* reply_guess = guess_reply(hidden_word, guess);
            strcpy(reply + 3, reply_guess);
            free(reply_guess);
        }

        else {
            *(reply) = 'N';
            strcpy(reply + 3, "?????");
        }

        short send_num_guesses = htons(num_guesses);
        *(short*)(reply + 1) = send_num_guesses;
        
        if (*(reply) == 'Y') {
            if (num_guesses != 1) {
                printf("THREAD %lu: sending reply: %s (%d guesses left)\n", 
                (unsigned long)pthread_self(), reply + 3, num_guesses);
            } 

            else {
                printf("THREAD %lu: sending reply: %s (%d guess left)\n", 
                (unsigned long)pthread_self(), reply + 3, num_guesses);
            }
        }

        else {
            if (num_guesses != 1) {
                printf("THREAD %lu: invalid guess; sending reply: %s (%d guesses left)\n", 
                (unsigned long)pthread_self(), reply + 3, num_guesses);
            } 

            else {
                printf("THREAD %lu: invalid guess; sending reply: %s (%d guess left)\n", 
                (unsigned long)pthread_self(), reply + 3, num_guesses);
            }    
        }

        send(client, reply, 9, 0);

        if (same_word(hidden_word, reply + 3) != 0) {
            win = 1;
            free(reply);
            break;
        }

        free(reply);

        if (num_guesses == 0) {
            break;
        }

        printf("THREAD %lu: waiting for guess\n", (unsigned long)pthread_self());
        n = recv(client, guess, 5, MSG_WAITALL);

        if (n == 0) {
            printf("THREAD %lu: client gave up; closing TCP connection...\n", (unsigned long)pthread_self());
            break;
        }

        else if (n < 0) {
            perror("ERROR: recv() failed!");
            break;
        }
    }

    printf("THREAD %lu: game over; word was %s!\n", (unsigned long)pthread_self(), hidden_word);

    if (win > 0) {
        pthread_mutex_lock(&mutex);
        total_wins++;
        pthread_mutex_unlock(&mutex);
    }

    else {
        pthread_mutex_lock(&mutex);
        total_losses++;
        pthread_mutex_unlock(&mutex);
    }

    free(guess);
    close(client);
    pthread_exit(NULL);
}

int wordle_server(int argc, char** argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    signal(SIGUSR1, handle_signal);

    if (argc != 5) {
        fprintf(stderr, "ERROR: Invalid argument(s)\nUSAGE: hw3.out <listener-port> <seed> <dictionary-filename> <num-dictionary>\n");
        return EXIT_FAILURE;
    }

    if (!is_int(*(argv + 1)) || !is_int(*(argv + 2)) || !is_int(*(argv + 4))) {
        fprintf(stderr, "ERROR: Invalid argument(s)\nUSAGE: hw3.out <listener-port> <seed> <dictionary-filename> <num-dictionary>\n");
        return EXIT_FAILURE;
    }

    dict_size = atoi(*(argv + 4));
    int dict_file = open(*(argv + 3), O_RDONLY);

    if (dict_file == -1) {
        fprintf(stderr, "ERROR: Invalid argument(s)\nUSAGE: hw3.out <listener-port> <seed> <dictionary-filename> <num-dictionary>\n");
        close(dict_file);
        return EXIT_FAILURE;
    }

    if (valid_dict_file(dict_file, dict_size) == 0) {
        free_dictionary();
        fprintf(stderr, "ERROR: Invalid argument(s)\nUSAGE: hw3.out <listener-port> <seed> <dictionary-filename> <num-dictionary>\n");
        close(dict_file);
        return EXIT_FAILURE;
    }

    close(dict_file);

    if (atoi(*(argv + 4)) != 1) {
        printf("MAIN: opened %s (%s words)\n", *(argv + 3), *(argv + 4));
    }

    else {
        printf("MAIN: opened %s (%s word)\n", *(argv + 3), *(argv + 4));
    }
    
    printf("MAIN: seeded pseudo-random number generator with %s\n", *(argv + 2));
    printf("MAIN: Wordle server listening on port {%s}\n", *(argv + 1));

    listener = socket(AF_INET, SOCK_STREAM, 0);

    if (listener == -1) {
        perror("socket() failed");
        return EXIT_FAILURE;
    }

    struct sockaddr_in tcp_server;
    tcp_server.sin_family = AF_INET;

    tcp_server.sin_addr.s_addr = htonl(INADDR_ANY);

    unsigned short port = atoi(*(argv + 1));
    tcp_server.sin_port = htons(port);

    if (bind(listener, (struct sockaddr *)&tcp_server, sizeof(tcp_server)) == -1) {
        perror("bind() failed");
        return EXIT_FAILURE;
    }

    if (listen(listener, 5) == -1) {
        perror("listen() failed");
        return EXIT_FAILURE;
    }

    srand(atoi(*(argv + 2)));
    words_size = 0;
    total_guesses = 0;
    total_losses = 0;
    total_wins = 0;

    while (1) {
        struct sockaddr_in remote_client;
        int addrlen = sizeof(remote_client);

        int newsd = accept(listener, (struct sockaddr*)&remote_client, (socklen_t *)&addrlen);

        if (newsd == -1) {
            perror("accept() failed");
            free_dictionary();
            return EXIT_SUCCESS;
        }

        pthread_t child_thread;

        if (pthread_create(&child_thread, NULL, (void*)handle_client, &newsd) != 0) {
            perror("pthread_create() failed");
            close(newsd);
            continue;
        }

        else {
            pthread_detach(child_thread);
        }
    }

    pthread_mutex_destroy(&mutex);

    return EXIT_SUCCESS;
}
