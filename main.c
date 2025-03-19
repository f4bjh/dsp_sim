#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h> // Pour sleep()
#include "bus.h"
#include "gpio.h"

#define NUM_REGS 16
#define MEM_SIZE 1024
#define PIPELINE_DEPTH 4  // Fetch, Decode, Execute, Writeback
#define MAX_LINE_LENGTH 100

// Définition des registres
int regs[NUM_REGS];

// Registre d'état
typedef struct {
    int carry;
    int zero;
    int negative;
} StatusRegister;

StatusRegister status;

// Enumération des instructions
typedef enum {
    NOP, LOAD, STORE, ADD, MUL, MAC, OUT, HALT, INVALID
} InstructionSet;

// Structure d'une instruction
typedef struct {
    InstructionSet opcode;
    int operand1;
    int operand2;
    int operand3;
    int immediate; // Indique si operand2 est une valeur immédiate
} Instruction;

// Programme en mémoire
Instruction program[MEM_SIZE];

// Étapes du pipeline
typedef struct {
    Instruction instr;
    int valid;
} PipelineStage;

PipelineStage pipeline[PIPELINE_DEPTH];

// Fonction de mappage texte -> opcode
InstructionSet get_opcode(const char* mnemonic) {
    if (strcmp(mnemonic, "LOAD") == 0) return LOAD;
    if (strcmp(mnemonic, "STORE") == 0) return STORE;
    if (strcmp(mnemonic, "ADD") == 0) return ADD;
    if (strcmp(mnemonic, "MUL") == 0) return MUL;
    if (strcmp(mnemonic, "MAC") == 0) return MAC;
    if (strcmp(mnemonic, "OUT") == 0) return OUT;
    if (strcmp(mnemonic, "HALT") == 0) return HALT;
    return INVALID;
}

// Fonction pour convertir une valeur immédiate (hex ou décimal)
int parse_immediate(const char* str) {
    int value;
    if (strstr(str, "0x") || strstr(str, "0X")) {
        sscanf(str, "%x", &value);  // Hexadécimal
    } else {
        sscanf(str, "%d", &value);  // Décimal
    }
    return value;
}

// Supprimer les espaces en début et fin de chaîne
void trim(char* str) {
    char* end;

    // Supprimer les espaces en début de chaîne
    while (isspace((unsigned char)*str)) str++;

    // Supprimer les espaces en fin de chaîne
    if (*str == 0) return;  // Chaîne vide

    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    *(end + 1) = 0;
}

// Mise à jour du registre d'état
void update_status(int result) {
    status.zero = (result == 0);
    status.negative = (result < 0);
    status.carry = (result > 0x7FFFFFFF || result < (int)0x80000000);
}

// Chargement du programme depuis un fichier
void load_program(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Erreur : Impossible d'ouvrir %s\n", filename);
        exit(1);
    }
    char line[MAX_LINE_LENGTH];
    int instr_count = 0;
    while (fgets(line, sizeof(line), file) && instr_count < MEM_SIZE) {
        char mnemonic[10], imm_str[20];
        int op1 = 0, op2 = 0, op3 = 0;
        int immediate = 0;

        // Supprimer les retours à la ligne
        line[strcspn(line, "\r\n")] = 0;
        trim(line);  // Supprime les espaces avant et après

        // Ignorer les commentaires
        char* comment = strstr(line, "//");
        if (comment) *comment = '\0';

        trim(line);  // Vérifier après suppression des commentaires
        if (strlen(line) == 0) continue;  // Ignorer les lignes vides

        // Vérifier la présence d'une valeur immédiate #
        char* imm_char = strchr(line, '#');
        if (imm_char) {
            sscanf(line, "%s R%d, #%s", mnemonic, &op1, imm_str);
            op2 = parse_immediate(imm_str);
            immediate = 1;
        } else {
            sscanf(line, "%s R%d, R%d, R%d", mnemonic, &op1, &op2, &op3);
        }

        InstructionSet opcode = get_opcode(mnemonic);
        if (opcode == INVALID) {
            printf("Erreur : Instruction inconnue -> %s\n", line);
            continue;
        }

        program[instr_count++] = (Instruction){opcode, op1, op2, op3, immediate};
    }
    fclose(file);
}

// Initialisation du pipeline
void init_pipeline() {
    for (int i = 0; i < PIPELINE_DEPTH; i++) {
        pipeline[i].valid = 0;
    }
    status.carry = status.zero = status.negative = 0;
}

// Avancer le pipeline
void advance_pipeline() {
    for (int i = PIPELINE_DEPTH - 1; i > 0; i--) {
        pipeline[i] = pipeline[i - 1];
    }
    pipeline[0].valid = 0;
}

// Exécution avec pipeline
void execute_pipeline() {
    int pc = 0;
    int running = 1;
    init_pipeline();
    while (running) {
        if (pc < MEM_SIZE && !pipeline[0].valid) {
            pipeline[0].instr = program[pc++];
            pipeline[0].valid = 1;
        }
        
        if (pipeline[2].valid) {
            Instruction instr = pipeline[2].instr;
            int result;
            switch (instr.opcode) {
                case LOAD:
                    if (instr.immediate)
                        regs[instr.operand1] = instr.operand2; // Valeur immédiate (décimale ou hex)
                    else
                        regs[instr.operand1] = read_from_bus(instr.operand2);
                    update_status(regs[instr.operand1]);
                    break;
                case STORE:
                    write_to_bus(regs[instr.operand1], regs[instr.operand2]);
                    break;
                case ADD:
                    result = regs[instr.operand2] + regs[instr.operand3];
                    regs[instr.operand1] = result;
                    update_status(result);
                    break;
                case MUL:
                    result = regs[instr.operand2] * regs[instr.operand3];
                    regs[instr.operand1] = result;
                    update_status(result);
                    break;
                case MAC:
                    result = regs[instr.operand1] + regs[instr.operand2] * regs[instr.operand3];
                    regs[instr.operand1] = result;
                    update_status(result);
                    break;
                case OUT:
                    gpio_write(regs[instr.operand1], regs[instr.operand2]);
                    break;
                case HALT:
                    running = 0;
                    break;
                default:
                    printf("Erreur : Instruction inconnue !\n");
                    running = 0;
            }
        }
        
        advance_pipeline();
        sleep(1); // Simulation du temps d'horloge
    }
}

int main() {
    load_program("program.txt");
    execute_pipeline();
    return 0;
}
