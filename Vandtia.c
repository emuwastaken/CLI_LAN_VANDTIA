#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define DECK_SIZE 56
#define MAX_NAME_LENGTH 50
#define MAX_PLAYERS 2
#define SERVER 1
#define CLIENT 2
#define PORT 8080


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





void start_client(char* server_ip) {
    struct sockaddr_in address;
    int sock = 0;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return;
    }

    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, server_ip, &(serv_addr.sin_addr)) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return;
    }
}

void start_server() {
    // Declare server file descriptor and new socket integer identifiers
    int server_fd, new_socket;
    // Declare a sockaddr_in structure to hold server details
    struct sockaddr_in address;
    // Integer for setsockopt
    int opt = 1;
    // Store the size of the address structure
    int addrlen = sizeof(address);

    // Create a new TCP/IP socket and assign it's file descriptor to server_fd
    // If there's an error (socket() returns 0), print an error message and exit
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Set options on the socket (enable the socket to reuse the address and port)
    // If there's an error, print an error message and exit
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    
    // Set the server's address family to AF_INET (IPv4)
    address.sin_family = AF_INET;
    // Set the server's IP address to INADDR_ANY (allow any IP to connect)
    address.sin_addr.s_addr = INADDR_ANY;
    // Set the server's port number, convert it from host byte order to network byte order
    address.sin_port = htons( PORT );

    // Bind the server socket to the specified IP and port
    // If there's an error, print an error message and exit
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Set the socket to passive mode with a backlog of 3 connections
    // If there's an error, print an error message and exit
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // Accept a new connection and assign it to new_socket
    // If there's an error, print an error message and exit
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }
}


struct Card *current_hand;
struct Card *legal_moves;

struct Card deck[DECK_SIZE];
int top_of_deck = 0;

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
void 

int main(int argc, char* argv[]) 
{
    //Initializes the machine's config.
    Config config = {"", 0, ""};
    
    static struct option long_options[] = {
        {"name", required_argument, 0, 'n'},
        {"mode", required_argument, 0, 'm'},
        {"ip",   required_argument, 0, 'i'},
        {0,      0,                 0,  0 }
    };

    int opt;
    char temp_ip[INET6_ADDRSTRLEN] = "";
    bool is_server_mode = false;

    while((opt = getopt_long(argc, argv, "n:m:i:", long_options, NULL)) != -1) {
        switch(opt) {
            case 'n':
                strncpy(config.name, optarg, MAX_NAME_LENGTH);
                break;
            case 'm':
                if(strcmp(optarg, "server") == 0) {
                    config.mode = SERVER;
                    is_server_mode = true;
                } else if(strcmp(optarg, "client") == 0) {
                    config.mode = CLIENT;
                }
                break;
            case 'i':
                strncpy(temp_ip, optarg, INET6_ADDRSTRLEN);
                break;
        }
    }

    if (!is_server_mode) {
        strncpy(config.ip, temp_ip, INET6_ADDRSTRLEN);
    }

    if (config.mode == SERVER) {
        start_server();
    } else if (config.mode == CLIENT) {
        start_client(config.ip);
    }
    return 0;
}