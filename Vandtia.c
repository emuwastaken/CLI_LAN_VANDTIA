#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/un.h>

#define DECK_SIZE 56
#define MAX_NAME_LENGTH 50
#define MAX_PLAYERS 4
#define SERVER 1
#define CLIENT 2
#define CLIENTS 3
#define PORT 6660
#define INITIAL_HAND_SIZE 3
#define SOCKET_NAME "/tmp/server0.sock"


typedef enum { CLUBS, DIAMONDS, HEARTS, SPADES, JOKER } Suit;
typedef enum { NUM, UTIL } Type;

typedef struct Card {
    Suit suit;
    Type type;
    int value;
} Card;

typedef struct Player {
    char name[MAX_NAME_LENGTH];
    Card *hand;
    size_t hand_size;
    Card hidden_cards[3];
    size_t hidden_size;
    Card visible_cards[3];
    size_t visable_size;
    int id;
    char ip[INET6_ADDRSTRLEN];
} Player;

typedef struct GameState {
    Card *deck;
    size_t deck_size;
    Card *game_stack;
    size_t game_stack_size;
    Card *discard_pile;
    size_t discard_pile_size;
    Player players[MAX_PLAYERS];
} GameState;

//Used to store CLI arguments at execution
typedef struct Config {
    char name[MAX_NAME_LENGTH];
    int mode;
    char ip[INET6_ADDRSTRLEN];
} Config;

struct Card *current_hand;
struct Card *legal_moves;

struct Card deck[DECK_SIZE];
int top_of_deck = 0;

void shuffle_deck(Card *deck, size_t deck_size) {
    if(deck_size > 1) {
        for(size_t i = 0; i < deck_size - 1; i++) {
            size_t j = i + rand() / (RAND_MAX / (deck_size - i) + 1);
            Card t = deck[j];
            deck[j] = deck[i];
            deck[i] = t;
        }
    }
}
void initialize_deck() {
    /* Initialization of the deck... */
    /* This is where you would populate the deck with cards. */
    /* For example: */
    for (int i = 0; i < DECK_SIZE; i++) {
        struct Card new_card;
        if (i < 13) {
            new_card.suit = HEARTS;
            new_card.value = i % 13 + 1; // +1 since cards values start from 1 not 0
        } else if (i < 26) {
            new_card.suit = DIAMONDS;
            new_card.value = i % 13 + 1;
        } else if (i < 39) {
            new_card.suit = CLUBS;
            new_card.value = i % 13 + 1;
        } else if (i < 52) {
            new_card.suit = SPADES;
            new_card.value = i % 13 + 1;
        } else {
            new_card.suit = JOKER;
            new_card.value = 1; // Let's say that Jokers have a value of 1
        }
        deck[i] = new_card;
    }
    
    /* Remember to shuffle the deck! */
}
void initialize_hand() {
    current_hand = malloc(INITIAL_HAND_SIZE * sizeof(struct Card));
    legal_moves = malloc(INITIAL_HAND_SIZE * sizeof(struct Card));

    if (current_hand == NULL || legal_moves == NULL) {
        fprintf(stderr, "Failed to allocate memory for hand/legal moves\n");
        exit(1);
    }
}
void draw_card_logic(struct Card card) {
    static int current_hand_size = INITIAL_HAND_SIZE;

    /* Check if we need to expand the hand */
    int i;
    for (i = 0; i < current_hand_size; i++) {
        if (current_hand[i].value == 0) {  // 0 indicates an empty spot
            break;
        }
    }

    /* If the hand is full, expand it */
    if (i == current_hand_size) {
        current_hand_size *= 2;
        current_hand = realloc(current_hand, current_hand_size * sizeof(struct Card));

        if (current_hand == NULL) {
            fprintf(stderr, "Failed to allocate memory for hand\n");
            exit(1);
        }
    }

    /* Add the card to the hand */
    current_hand[i] = card;
}
void clean_up() {
    free(current_hand);
    free(legal_moves);
}
void black_joker_logic(GameState *gameState) {
    shuffle_deck(gameState->game_stack, gameState->game_stack_size);
    
    size_t half = gameState->game_stack_size / 2;
    gameState->discard_pile = realloc(gameState->discard_pile, 
        sizeof(Card) * (gameState->discard_pile_size + half));

    for(size_t i = 0; i < half; i++) {
        gameState->discard_pile[gameState->discard_pile_size + i] = 
            gameState->game_stack[gameState->game_stack_size - i - 1];
    }

    gameState->discard_pile_size += half;
    gameState->game_stack_size -= half;
    
    gameState->deck = realloc(gameState->deck, 
        sizeof(Card) * (gameState->deck_size + gameState->game_stack_size));

    for(size_t i = 0; i < gameState->game_stack_size; i++) {
        gameState->deck[gameState->deck_size + i] = 
            gameState->game_stack[i];
    }

    gameState->deck_size += gameState->game_stack_size;
    gameState->game_stack_size = 0;

    shuffle_deck(gameState->deck, gameState->deck_size);
}
void two_card_logic(GameState *gameState){

}
void ten_card_logic(GameState *gameState){

}
char *append_input(char *existing_input, const char *new_input) {
    size_t existing_input_len = strlen(existing_input);
    size_t new_input_len = strlen(new_input);
    size_t total_len = existing_input_len + new_input_len + 2;

    existing_input = realloc(existing_input, total_len * sizeof(char));
    if (existing_input == NULL) {
        fprintf(stderr, "Error reallocating memory!\n");
        return NULL;
    }

    strcat(existing_input, ", ");
    strcat(existing_input, new_input);

    return existing_input;
}
char *read_input(int sock) {
    char buffer[1024] = {0};
    read(sock , buffer, 1024);

    char *input = malloc((strlen(buffer) + 1) * sizeof(char));
    if (input == NULL) {
        fprintf(stderr, "Error allocating memory!\n");
        return NULL;
    }

    strcpy(input, buffer);
    return input;
}
void read_and_send_input(int sock, char **input_buffer_ptr) {
    char new_input[256];
    fgets(new_input, 256, stdin);
    *input_buffer_ptr = append_input(*input_buffer_ptr, new_input);
    write(sock , *input_buffer_ptr , strlen(*input_buffer_ptr));
}

// Global flag to indicate whether the host has issued the 'start' command.
volatile int startFlag = 0;

// Function for the thread to run.
// It reads from standard input and sets the start flag if 'start' is typed.
void* commandListener(void* arg) {
    char command[256];
    while (1) {
        fgets(command, sizeof(command), stdin);
        // If the command 'start' is detected, the startFlag is set to 1
        // and the thread returns
        if (strncmp(command, "start", 5) == 0) {
            startFlag = 1;
            return NULL;
        }
    }
}

void socket_clean_up(int tcpSock, int unixSock, char* sockName) {
    printf("Cleaning up...\n");
    close(tcpSock);
    close(unixSock);
    unlink(sockName);
}

char* get_unix_socket_name() {
    static int counter = 0;
    char* name = malloc(20);
    sprintf(name, "/tmp/server%d.sock", counter++);
    return name;
}

void start_server() {
    // Declare necessary socket and address variables
    int server_fd, new_socket, unix_socket_fd, unix_conn_socket;
    struct sockaddr_in address;
    struct sockaddr_un unix_address;
    int opt = 1;
    int addrlen = sizeof(address);
    char *socket_name = "/tmp/server0_socket";

    // Creating a Unix domain socket for inter-process communication
    printf("Creating Unix Domain Socket for server0\n");
    if ((unix_socket_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket error");
        exit(EXIT_FAILURE);
    }

    // Prepare Unix socket address structure
    memset(&unix_address, 0, sizeof(struct sockaddr_un));
    unix_address.sun_family = AF_UNIX;
    strncpy(unix_address.sun_path, socket_name, sizeof(unix_address.sun_path) - 1);

    // Binding Unix domain socket to the socket address
    printf("Binding Unix Domain Socket for server0\n");
    if (bind(unix_socket_fd, (struct sockaddr*)&unix_address, sizeof(struct sockaddr_un)) == -1) {
        perror("bind error");
        socket_clean_up(server_fd, unix_socket_fd, socket_name);
        exit(EXIT_FAILURE);
    }

    // Listening for incoming IPC connections on the Unix domain socket
    printf("Listening Unix Domain Socket for server0\n");
    if (listen(unix_socket_fd, 5) == -1) {
        perror("listen error");
        exit(EXIT_FAILURE);
    }

    // Creating a separate thread to handle specific tasks (e.g., listening for commands)
    pthread_t cmdThread;
    if (pthread_create(&cmdThread, NULL, commandListener, NULL) != 0) {
        perror("Failed to create command listener thread");
        exit(EXIT_FAILURE);
    }

    // Create a TCP/IP socket for network communication
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Set socket options to allow re-use of the socket address and port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // Prepare TCP/IP socket address structure
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind TCP/IP socket to the socket address
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        socket_clean_up(server_fd, unix_socket_fd, socket_name);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming network connections on the TCP/IP socket
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // Main server loop
    while(1) {
        // Accept a new network connection
        new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if(new_socket < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        // Create a new child process for each connection
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork error");
            exit(EXIT_FAILURE);
        } else if (pid > 0) {  // Parent process
            // Wait for the child process to connect via IPC
            printf("Waiting for connection from child process...\n");
            if ((unix_conn_socket = accept(unix_socket_fd, NULL, NULL)) == -1) {
                perror("accept error");
                socket_clean_up(server_fd, unix_socket_fd, socket_name);
                exit(EXIT_FAILURE);
            }
            printf("Connection established with child process.\n");

            /* Insert code here to handle communication with child process */
        } else {  // Child process
            // Connect to the parent process via IPC
            printf("Connecting back to parent process...\n");
            int unix_conn_back_socket;
            if ((unix_conn_back_socket = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
                perror("socket error");
                exit(EXIT_FAILURE);
            }
            if (connect(unix_conn_back_socket, (struct sockaddr*)&unix_address, sizeof(struct sockaddr_un)) == -1) {
                perror("connect error");
                exit(EXIT_FAILURE);
            }
            printf("Connected back to parent process.\n");

            /* Insert code here to handle communication with parent process */

            /* Insert game loop code here */
            exit(EXIT_SUCCESS);
        }
    }
}



void start_client(char* server_ip, int port) {
    int sock = 0;
    struct sockaddr_in serv_addr;
  
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return;
    }
  
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
  
    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return;
    }
  
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return;
    }
    
    /* Client code goes here... */
}


void print_local_ip(){
    struct ifaddrs *ifAddrStruct = NULL;
    void *tmpAddrPtr = NULL;

    getifaddrs(&ifAddrStruct);

    while (ifAddrStruct != NULL) {
        if (ifAddrStruct->ifa_addr->sa_family == AF_INET) { // check it is IP4
            // is a valid IP4 Address
            tmpAddrPtr = &((struct sockaddr_in *)ifAddrStruct->ifa_addr)->sin_addr;
            char addressBuffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
            printf("%s IP Address %s\n", ifAddrStruct->ifa_name, addressBuffer); 
        } else if (ifAddrStruct->ifa_addr->sa_family == AF_INET6) { // check it is IP6
            // is a valid IP6 Address
            tmpAddrPtr = &((struct sockaddr_in *)ifAddrStruct->ifa_addr)->sin_addr;
            char addressBuffer[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, tmpAddrPtr, addressBuffer, INET6_ADDRSTRLEN);
            printf("%s IP Address %s\n", ifAddrStruct->ifa_name, addressBuffer); 
        } 
        ifAddrStruct = ifAddrStruct->ifa_next;
    }
}

// The main function starts here, accepting command-line arguments.
int main(int argc, char* argv[]) 
{
    // Initialize a config structure with default values.
    Config config = {"", 0, ""};
    
    // Define an array of option structs for getopt_long, specifying what command-line options we expect.
    static struct option long_options[] = {
        {"name", required_argument, 0, 'n'},  // 'name' option expects an argument, maps to 'n'
        {"mode", required_argument, 0, 'm'},  // 'mode' option expects an argument, maps to 'm'
        {"ip",   required_argument, 0, 'i'},  // 'ip' option expects an argument, maps to 'i'
        {0,      0,                 0,  0 }   // Null-terminated list
    };

    // Variable to store the current option.
    int opt;
    // While there are still options to be parsed,
    while((opt = getopt_long(argc, argv, "n:m:i:", long_options, NULL)) != -1) {
        // Depending on the current option,
        switch(opt) {
            case 'n':
                // If it's 'n', copy the argument to the config name.
                strncpy(config.name, optarg, MAX_NAME_LENGTH);
                break;
            case 'm':
                // If it's 'm', check the argument and set the mode accordingly.
                if(strcmp(optarg, "server") == 0) {
                    config.mode = SERVER;
                } else if(strcmp(optarg, "client") == 0) {
                    config.mode = CLIENT;
                }
                break;
            case 'i':
                // If it's 'i', copy the argument to the config IP.
                strncpy(config.ip, optarg, INET6_ADDRSTRLEN);
                break;
        }
    }

    
    if (config.mode == '\0') {
        fprintf(stderr, "TCP connection requires the -m argument, client || host.\n");
        return 1;
    }
    // If we're in client mode and no IP was given, print an error message and exit.
    if (config.mode == CLIENT && config.ip[0] == '\0') {
        fprintf(stderr, "Client mode requires the -ip argument.\n");
        return 1;
    }

    // Depending on the mode, start the server or client.
    if (config.mode == SERVER) {
        print_local_ip();
        printf("Starting server code\n");
        start_server();
    } else if (config.mode == CLIENT) {
        //Tries to connect to any of the 3 servers active
        for(int i=0; i<CLIENTS; i++){
            printf("Attempting to connect to server%d on port %d\n",i, (PORT+i));
            start_client(config.ip, PORT+i);
        }
        
    }

    // Exit normally.
    return 0;
}
