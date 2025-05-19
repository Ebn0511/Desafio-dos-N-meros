#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ncurses.h>
#include <curl/curl.h>
#include "cJSON.h"
#include <ctype.h>
#include "gemini.h"

#define MAX_NOME 50
#define MAX_ENIGMAS 100

// ----------------- Estrutura para resposta da API -----------------
typedef struct {
    char *buffer;
    size_t size;
} MemoryStruct;

// ----------------- Estrutura de Pilha para enigmas -----------------
typedef struct Enigma {
    char texto[256];
    struct Enigma* prox;
} Enigma;

Enigma* topo = NULL;

void empilharEnigma(const char* texto) {
    Enigma* novo = (Enigma*)malloc(sizeof(Enigma));
    strncpy(novo->texto, texto, 255);
    novo->texto[255] = '\0';
    novo->prox = topo;
    topo = novo;
}

char* desempilharEnigma() {
    if (!topo) return NULL;
    Enigma* temp = topo;
    topo = topo->prox;
    char* texto = strdup(temp->texto);
    free(temp);
    return texto;
}

int enigmaJaUsadoPilha(const char* novo) {
    Enigma* atual = topo;
    while (atual) {
        if (strcmp(atual->texto, novo) == 0) return 1;
        atual = atual->prox;
    }
    return 0;
}

// ----------------- Estrutura de Ranking: Lista Circular Duplamente Encadeada -----------------
typedef struct Jogador {
    char nome[MAX_NOME];
    int pontuacao;
    struct Jogador* ant;
    struct Jogador* prox;
} Jogador;

Jogador* ranking = NULL;

void inserirRanking(char nome[], int pontuacao) {
    Jogador* novo = (Jogador*)malloc(sizeof(Jogador));
    strcpy(novo->nome, nome);
    novo->pontuacao = pontuacao;
    if (!ranking) {
        novo->prox = novo;
        novo->ant = novo;
        ranking = novo;
    } else {
        Jogador* ultimo = ranking->ant;
        novo->prox = ranking;
        novo->ant = ultimo;
        ranking->ant = novo;
        ultimo->prox = novo;
    }
}

void ordenarRanking() {
    if (!ranking || ranking->prox == ranking) return;
    int trocou;
    do {
        trocou = 0;
        Jogador* atual = ranking;
        do {
            Jogador* proximo = atual->prox;
            if (atual->pontuacao < proximo->pontuacao) {
                char tmpNome[MAX_NOME];
                int tmpPontuacao;
                strcpy(tmpNome, atual->nome);
                tmpPontuacao = atual->pontuacao;
                strcpy(atual->nome, proximo->nome);
                atual->pontuacao = proximo->pontuacao;
                strcpy(proximo->nome, tmpNome);
                proximo->pontuacao = tmpPontuacao;
                trocou = 1;
            }
            atual = atual->prox;
        } while (atual->prox != ranking);
    } while (trocou);
}

void mostrarRanking() {
    clear();
    printw("\n==== RANKING ====");
    if (!ranking) {
        printw("\nRanking vazio.");
    } else {
        Jogador* atual = ranking;
        int pos = 1;
        do {
            printw("\n%d. %s - %d pontos", pos++, atual->nome, atual->pontuacao);
            atual = atual->prox;
        } while (atual != ranking);
    }
    printw("\n\nPressione qualquer tecla para voltar ao menu.");
    getch();
}

void limparString(char *str) {
    char *src = str, *dst = str;
    while (*src) {
        if (!isspace((unsigned char)*src)) {
            *dst++ = tolower((unsigned char)*src);
        }
        src++;
    }
    *dst = '\0';
}

int compararRespostas(const char *a, const char *b) {
    char copiaA[100], copiaB[100];
    strncpy(copiaA, a, sizeof(copiaA) - 1);
    strncpy(copiaB, b, sizeof(copiaB) - 1);
    copiaA[sizeof(copiaA) - 1] = '\0';
    copiaB[sizeof(copiaB) - 1] = '\0';
    limparString(copiaA);
    limparString(copiaB);
    return strcmp(copiaA, copiaB) == 0;
}

void limparRespostaIA(char* resposta) {
    char* src = resposta;
    char* dst = resposta;
    while (*src) {
        if (strncmp(src, "```", 3) == 0) {
            src += 3;
            continue;
        }
        if (strncmp(src, "text", 4) == 0) {
            src += 4;
            continue;
        }
        if (*src == '\n') {
            src++;
            continue;
        }
        *dst++ = *src++;
    }
    *dst = '\0';
}
void extrairNumerosApenas(char* destino, const char* origem) {
    int j = 0;
    for (int i = 0; origem[i] != '\0'; i++) {
        if (isdigit(origem[i])) {
            destino[j++] = origem[i];
        }
    }
    destino[j] = '\0';
}

void jogarDesafio() {
    clear();
    printw("Digite seu nome: ");
    char nome[MAX_NOME];
    echo();
    getstr(nome);
    noecho();

    printw("Olá, %s! Pressione qualquer tecla para continuar...\n", nome);
    getch();

    int pontuacao = 0;

    while (1) {
        clear();
        const char* promptDesafio = "Me envie apenas 1 enigma matematico simples e objetivo para um jogo educativo infantil. Apenas o enunciado.";
        char* desafio = NULL;

        int tentativas = 0;
        do {
            if (desafio) {
                free(desafio);
                desafio = NULL;
            }

            desafio = chamarIA(promptDesafio);

            if (!desafio || strlen(desafio) < 3) {
                printw("❌ Erro ao obter enigma da IA. Tentando novamente...\n");
                getch();
                tentativas++;
                continue;
            }

            if (enigmaJaUsadoPilha(desafio)) {
                printw("⚠️ Enigma repetido detectado. Buscando outro...\n");
                getch();
                tentativas++;
                free(desafio);
                desafio = NULL;
                continue;
            }

            break;

        } while (tentativas < 3);

        if (!desafio || tentativas >= 3) {
            printw("Erro ao obter desafio único da IA. Encerrando partida.\n");
            getch();
            return;
        }

        empilharEnigma(desafio);

        printw("Desafio para %s:\n\n", nome);
        printw("%s\n", desafio);
        printw("\n(Saia digitando 'q')\nSua resposta: ");

        char respostaJogador[100];
        echo();
        memset(respostaJogador, 0, sizeof(respostaJogador));
        getnstr(respostaJogador, sizeof(respostaJogador) - 1);
        noecho();

        if (strcmp(respostaJogador, "q") == 0) {
            free(desafio);
            break;
        }

        if (strlen(respostaJogador) == 0) {
            printw("\n⚠️ Nenhuma resposta digitada! Tente novamente.\n");
            free(desafio);
            continue;
        }

        char promptResposta[512];
        snprintf(promptResposta, sizeof(promptResposta), "Resolva este enigma apenas com a resposta numerica, apenas a resposta em digito, sem explicar nada: %s", desafio);
        free(desafio);
        char* respostaIA = chamarIA(promptResposta);

        if (!respostaIA || strlen(respostaIA) < 1) {
            printw("Erro ao obter resposta da IA.\n");
            if (respostaIA) free(respostaIA);
            getch();
            return;
        }

        // Extração limpa da resposta numérica
        char respostaEsperada[20] = "";
        extrairNumerosApenas(respostaEsperada, respostaIA);

        if (compararRespostas(respostaEsperada, respostaJogador)) {
            pontuacao++;
            printw("\n✅ Resposta correta! Pontuação: %d\n", pontuacao);
        } else {
            printw("\n❌ Resposta incorreta. Resposta esperada: %s\n", respostaEsperada);
            refresh();
            napms(2500);
            free(respostaIA);
            break;
        }

        free(respostaIA);
        refresh();
        napms(1500);
    }

    inserirRanking(nome, pontuacao);
    ordenarRanking();
    printw("\n\nFim de jogo! Sua pontuação: %d\n", pontuacao);
    printw("Pressione qualquer tecla para voltar ao menu.");
    getch();
}



void menu() {
    int opcao;
    while (1) {
        clear();
        printw("=== DESAFIO DO MESTRE DOS NÚMEROS ===\n");
        printw("1. Jogar desafio\n");
        printw("2. Ver ranking\n");
        printw("3. Sair\n");
        printw("Escolha uma opção: ");

        opcao = getch();
        switch(opcao) {
            case '1': jogarDesafio(); break;
            case '2': mostrarRanking(); break;
            case '3': return;
            default: printw("\nOpção inválida. Tente novamente.\n"); getch();
        }
    }
}

int main() {
    srand(time(NULL));
    initscr();
    cbreak();
    noecho();
    menu();
    endwin();
    return 0;
}

