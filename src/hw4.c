#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT1 2201 // Player 1 port
#define PORT2 2202 // Player 2 port
#define BUFFER_SIZE 1024
#define SHIP_SIZE 4

typedef struct Board Board;
typedef struct Piece Piece;
typedef struct Player Player;

void initializeBoard(Board *board, int width, int height);
void deleteBoard(Board *board);
int setup_server(int port);
int accept_client(int server_fd);
int write_data(int client_fd, const char *data);
int processCommands(int client_fd, char *buffer, int size);
void handleShipPlacement(int client_fd, Player *player, const char *packet);
int isValidPlacement(Board *board, Piece *piece);
void placeShip(Board *board, Piece *piece);
void acknowledgePackage(int client_fd);
void sendError(int client_fd, int error_code);
int **createShip(int type, int rotation);
void freeShip(int **shape);
int beginPacket(int client_fd, const char *packet, Player *player1, Player *player2);
void handleInitializePacket(int client_fd, const char *packet, Player *player);
void handleShootPacket(int client_fd, const char *packet, Player *player, Player *opponent);

struct Board {
    int width;
    int height;
    char **cells;
};

struct Piece {
    int type;
    int rotation;
    int column, row;
};

struct Player {
    Board board;
    Piece ships[5];
    int shipCount;
    int shipsRemaining;
    int client_fd;
};

void initializeBoard(Board *board, int width, int height) {
    board->width = width;
    board->height = height;
    board->cells = (char **)malloc(height * sizeof(char *));
    if (!board->cells) {
        perror("Failed to allocate memory for board cells");
        exit(EXIT_FAILURE);  // Handle allocation failure
    }
    for (int i = 0; i < height; i++) {
        board->cells[i] = (char *)malloc(width * sizeof(char));
        if (!board->cells[i]) {
            perror("Failed to allocate memory for row");
            exit(EXIT_FAILURE);  // Handle allocation failure
        }
        memset(board->cells[i], 0, width);  // Initialize to 0 (empty)
    }
}

void deleteBoard(Board *board) {
    for (int i = 0; i < board->height; i++) {
        free(board->cells[i]);
    }
    free(board->cells);
}

void acknowledgePackage(int client_fd) {
    write_data(client_fd, "A\n");
}

void sendError(int client_fd, int error_code) {
    char error_msg[BUFFER_SIZE]; // E followed by xxx (100-400)
    snprintf(error_msg, sizeof(error_msg), "E %d\n", error_code);
    write_data(client_fd, error_msg);
}

int beginPacket(int client_fd, const char *packet, Player *player1, Player *player2) {
    int width, height;
    if (sscanf(packet, "B %d %d", &width, &height) == 2) {
        if (width >= 10 && height >= 10) {
            initializeBoard(&player1->board, width, height);
            initializeBoard(&player2->board, width, height);
            player1->shipsRemaining = 5;
            player2->shipsRemaining = 5;
            acknowledgePackage(client_fd);
            return 1;  // Valid Begin packet
        } else {
            sendError(client_fd, 200);  // Invalid Begin packet (invalid number of parameters or parameter out of range)
            return 0;
        }
    }
    sendError(client_fd, 100);  // Invalid packet type (Expected Begin)
    return 0;
}

int setup_server(int port) {
    int server_fd;
    struct sockaddr_in address;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 1) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    return server_fd;
}

int accept_client(int server_fd) {
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    int client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
    if (client_fd < 0) {
        perror("accept failed");
        exit(EXIT_FAILURE);
    }
    return client_fd;
}

int write_data(int client_fd, const char *data) {
    int len = strlen(data);
    if (write(client_fd, data, len) == -1) {
        perror("write failed");
        return -1;
    }
    return len;
}

int processCommands(int client_fd, char *buffer, int size) {
    int data = read(client_fd, buffer, size);
    if (data == -1) {
        perror("read failed");
        return -1;
    }
    if (data == 0) {  // Client disconnected
        printf("Client disconnected.\n");
        return -1;
    }
    buffer[data] = '\0';  // Ensure null-terminated string
    return data;
}

// Hard-coded Tetris ship shapes and their rotations
int tetrisPieces[7][4][4][2] = {
    // O Piece (1)
    {
        {{0, 0}, {1, 0}, {0, 1}, {1, 1}}, // All rotations are the same
        {{0, 0}, {1, 0}, {0, 1}, {1, 1}},
        {{0, 0}, {1, 0}, {0, 1}, {1, 1}},
        {{0, 0}, {1, 0}, {0, 1}, {1, 1}}
    },
    // I Piece (2)
    {
        {{0, 0}, {0, 1}, {0, 2}, {0, 3}}, // Rotation 0 (Vertical)
        {{0, 0}, {1, 0}, {2, 0}, {3, 0}}, // Rotation 1 (Horizontal)
        {{0, 0}, {0, 1}, {0, 2}, {0, 3}}, // Rotation 2 (Vertical)
        {{0, 0}, {1, 0}, {2, 0}, {3, 0}}  // Rotation 3 (Horizontal)
    },
    // S Piece (3)
    {
        {{0, 0}, {1, 0}, {1, 1}, {2, 1}}, // Rotation 0
        {{1, 0}, {1, 1}, {0, 1}, {0, 2}}, // Rotation 1
        {{0, 0}, {1, 0}, {1, 1}, {2, 1}}, // Rotation 2
        {{1, 0}, {1, 1}, {0, 1}, {0, 2}}  // Rotation 3
    },
    // L Piece (4)
    {
        {{0, 0}, {1, 0}, {2, 0}, {2, 1}}, // Rotation 0
        {{0, 1}, {1, 1}, {2, 1}, {2, 0}}, // Rotation 1
        {{0, 0}, {0, 1}, {1, 1}, {2, 1}}, // Rotation 2
        {{0, 0}, {0, 1}, {0, 2}, {1, 0}}  // Rotation 3
    },
    // Z Piece (5)
    {
        {{0, 0}, {1, 0}, {1, 1}, {2, 1}}, // Rotation 0
        {{0, 1}, {1, 1}, {1, 0}, {2, 0}}, // Rotation 1
        {{0, 0}, {1, 0}, {1, 1}, {2, 1}}, // Rotation 2
        {{0, 1}, {1, 1}, {1, 0}, {2, 0}}  // Rotation 3
    },
    // J Piece (6)
    {
        {{0, 0}, {0, 1}, {0, 2}, {1, 2}}, // Rotation 0
        {{0, 0}, {1, 0}, {2, 0}, {2, 1}}, // Rotation 1
        {{0, 0}, {1, 0}, {1, 1}, {1, 2}}, // Rotation 2
        {{0, 0}, {0, 1}, {1, 0}, {2, 0}}  // Rotation 3
    },
    // T Piece (7)
    {
        {{0, 0}, {1, 0}, {2, 0}, {1, 1}}, // Rotation 0
        {{1, 0}, {1, 1}, {1, 2}, {0, 1}}, // Rotation 1
        {{0, 1}, {1, 1}, {2, 1}, {1, 0}}, // Rotation 2
        {{1, 0}, {1, 1}, {1, 2}, {2, 1}}  // Rotation 3
    }
};

// Create the piece in a 2D array
int **createShip(int type, int rotation) {
    int **shape = (int **)malloc(SHIP_SIZE * sizeof(int *));
    if (!shape) {
        perror("Failed to allocate memory for ship shape");
        exit(EXIT_FAILURE);  // Handle allocation failure
    }

    for (int i = 0; i < SHIP_SIZE; i++) {
        shape[i] = (int *)malloc(2 * sizeof(int)); // Two columns (dx, dy)
        if (!shape[i]) {
            perror("Failed to allocate memory for ship shape part");
            exit(EXIT_FAILURE);  // Handle allocation failure
        }
    }

    int idx = 0;
    for (int i = 0; i < SHIP_SIZE; i++) {
        if (tetrisPieces[type][rotation][i][0] == 0 && tetrisPieces[type][rotation][i][1] == 0) break;
        shape[idx][0] = tetrisPieces[type][rotation][i][0];
        shape[idx][1] = tetrisPieces[type][rotation][i][1];
        idx++;
    }

    return shape;
}

void freeShip(int **shape) {
    for (int i = 0; i < SHIP_SIZE; i++) {
        free(shape[i]);
    }
    free(shape);
}
// Check if the piece can be placed within the bounds of the board
int isValidPlacement(Board *board, Piece *piece) {
    int **shape = createShip(piece->type, piece->rotation);

    for (int i = 0; i < SHIP_SIZE; i++) {
        int x = piece->column + shape[i][0];
        int y = piece->row + shape[i][1];

        if (x < 0 || x >= board->width || y < 0 || y >= board->height || board->cells[y][x] != 0) {
            freeShip(shape);
            return 0;  // Invalid placement (out of bounds or collision)
        }
    }

    freeShip(shape);
    return 1;  // Valid placement
}

// Place the ship on the board
void placeShip(Board *board, Piece *piece) {
    int **shape = createShip(piece->type, piece->rotation);

    for (int i = 0; i < SHIP_SIZE; i++) {
        int x = piece->column + shape[i][0];
        int y = piece->row + shape[i][1];

        board->cells[y][x] = 1;  // Mark cell as occupied
    }

    freeShip(shape);
}

// Handle the ship placement command from the client
void handleShipPlacement(int client_fd, Player *player, const char *packet) {
    int x, y, type, rotation;
    if (sscanf(packet, "S %d %d %d %d", &x, &y, &type, &rotation) == 4) {
        if (type < 0 || type >= 7) {
            sendError(client_fd, 300);  // Invalid ship type
            return;
        }

        Piece piece = {type, rotation, x, y};
        if (isValidPlacement(&player->board, &piece)) {
            player->ships[player->shipCount] = piece;
            player->shipCount++;
            placeShip(&player->board, &piece);
            acknowledgePackage(client_fd);  // Acknowledge ship placement
        } else {
            sendError(client_fd, 400);  // Invalid placement (out of bounds or overlap)
        }
    } else {
        sendError(client_fd, 100);  // Invalid packet format (Expecting S x y type rotation)
    }
}

void handleInitializePacket(int client_fd, const char *packet, Player *player) {
    int type, rotation, x, y;
    int i = 0;
    
    // Skip the "I" token
    const char *start = packet + 2; // to skip "I "

    // Loop through and parse the ships
    while (sscanf(start, "%d %d %d %d", &type, &rotation, &x, &y) == 4) {
        if (type < 0 || type >= 7) {
            sendError(client_fd, 300);  // Invalid ship type
            return;
        }

        // Initialize the ship
        Piece piece = {type, rotation, x, y};
        
        // Check for valid placement on the board
        if (isValidPlacement(&player->board, &piece)) {
            player->ships[player->shipCount] = piece;
            player->shipCount++;
            placeShip(&player->board, &piece);  // Place the ship on the board
        } else {
            sendError(client_fd, 400);  // Invalid placement (out of bounds or overlap)
            return;
        }

        // Move to the next ship in the packet
        while (*start != ' ' && *start != '\0') start++; // Skip to next space
        if (*start == '\0') break; // End of packet
        start++; // Skip the space
    }

    // Acknowledge the packet if all ships are placed
    acknowledgePackage(client_fd);
}

// Handle shooting packet (a player's shot attempt)
void handleShootPacket(int client_fd, const char *packet, Player *player, Player *opponent) {
    int x, y;

    if (sscanf(packet, "F %d %d", &x, &y) == 2) {
        if (x < 0 || x >= opponent->board.width || y < 0 || y >= opponent->board.height) {
            sendError(client_fd, 500);  // Invalid shot (out of bounds)
            return;
        }

        if (opponent->board.cells[y][x] == 1) {
            opponent->board.cells[y][x] = 2;  // Mark the shot cell as hit
            opponent->shipsRemaining--;

            // Check if the opponent has lost all ships
            if (opponent->shipsRemaining == 0) {
                write_data(client_fd, "W\n");  // Win message
            } else {
                write_data(client_fd, "H\n");  // Hit message
            }
        } else {
            write_data(client_fd, "M\n");  // Miss message
        }
    } else {
        sendError(client_fd, 100);  // Invalid packet format (Expecting F x y)
    }
}

// Main game loop to handle all incoming packets
void gameLoop(int client_fd1, int client_fd2, Player *player1, Player *player2) {
    char buffer[BUFFER_SIZE];
    int bytes_read;
    int turn = 0; // 0 for player 1, 1 for player 2
    int client_fds[2] = {client_fd1, client_fd2};  // Array of client file descriptors
    Player *players[2] = {player1, player2};      // Array of players

    while (1) {
        // Select current player and opponent based on the turn
        int current_player_fd = client_fds[turn];
        Player *current_player = players[turn];
        Player *opponent_player = players[1 - turn]; // The other player

        // Read command from the current player
        bytes_read = processCommands(current_player_fd, buffer, sizeof(buffer));
        if (bytes_read == -1) break;  // Client disconnected
        
        // Handle current player's packet
        if (strncmp(buffer, "B", 1) == 0) {
            if (beginPacket(current_player_fd, buffer, player1, player2) == 0) break;
        } else if (strncmp(buffer, "S", 1) == 0) {
            handleShipPlacement(current_player_fd, current_player, buffer);
        } else if (strncmp(buffer, "F", 1) == 0) {
            handleShootPacket(current_player_fd, buffer, current_player, opponent_player);
        }

        // Switch turn (toggle between 0 and 1)
        turn = 1 - turn;
    }
}

int main() {
    int server_fd1 = setup_server(PORT1);
    int server_fd2 = setup_server(PORT2);

    printf("Waiting for players to connect...\n");

    int client_fd1 = accept_client(server_fd1);
    int client_fd2 = accept_client(server_fd2);

    Player player1 = {{0}, {{0}}, 0, 5, client_fd1};
    Player player2 = {{0}, {{0}}, 0, 5, client_fd2};

    printf("Players connected. Starting game...\n");

    gameLoop(client_fd1, client_fd2, &player1, &player2);

    close(client_fd1);
    close(client_fd2);
    close(server_fd1);
    close(server_fd2);

    return 0;
}