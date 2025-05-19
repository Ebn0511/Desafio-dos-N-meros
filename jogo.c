#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ncurses.h>
#include <curl/curl.h>
#include "cJSON.h"
#include <ctype.h>
#include "gemini.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

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
void renderTexto(SDL_Renderer* renderer, TTF_Font* fonte, const char* texto, int x, int y, SDL_Color cor) {
    SDL_Surface* surface = TTF_RenderUTF8_Blended(fonte, texto, cor);
    if (!surface) return;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Rect dst = {x, y, surface->w, surface->h};

    SDL_RenderCopy(renderer, texture, NULL, &dst);

    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}
void renderTextoMultilinha(SDL_Renderer* renderer, TTF_Font* fonte, const char* textoOriginal, int x, int yInicial, SDL_Color cor) {
    const int larguraMaxima = 500;
    int yAtual = yInicial;

    char linha[1024] = "";
    char palavra[512] = "";
    const char* ptr = textoOriginal;
    int linhaLen = 0;

    while (*ptr) {
        int bytes = 1;

        // Detectar quantos bytes o caractere UTF-8 ocupa
        unsigned char c = (unsigned char)*ptr;
        if ((c & 0x80) == 0x00)       bytes = 1;
        else if ((c & 0xE0) == 0xC0)  bytes = 2;
        else if ((c & 0xF0) == 0xE0)  bytes = 3;
        else if ((c & 0xF8) == 0xF0)  bytes = 4;

        // Copiar a palavra atual
        int i = 0;
        while (*ptr && !isspace(*ptr)) {
            palavra[i++] = *ptr++;
            // verificar se estamos em meio a UTF-8 multibyte
            while ((unsigned char)*(ptr) >= 128 && (unsigned char)*(ptr) <= 191) {
                palavra[i++] = *ptr++;
            }
        }
        palavra[i] = '\0';

        // Pular espaços e adicionar ao final da palavra
        char espacos[16] = "";
        int j = 0;
        while (*ptr && isspace(*ptr)) {
            espacos[j++] = *ptr++;
        }
        espacos[j] = '\0';

        // Testar tamanho se essa palavra entrar na linha
        char teste[1024];
        snprintf(teste, sizeof(teste), "%s%s%s", linha, linhaLen > 0 ? " " : "", palavra);

        int largura;
        TTF_SizeUTF8(fonte, teste, &largura, NULL);

        if (largura > larguraMaxima) {
            // Renderiza a linha anterior
            renderTexto(renderer, fonte, linha, x, yAtual, cor);
            yAtual += 40;
            snprintf(linha, sizeof(linha), "%s", palavra);
        } else {
            if (linhaLen > 0) {
                strcat(linha, " ");
                strcat(linha, palavra);
            } else {
                strcpy(linha, palavra);
            }
        }

        linhaLen = strlen(linha);
    }

    if (linhaLen > 0) {
        renderTexto(renderer, fonte, linha, x, yAtual, cor);
    }
}






char* removerMarkdown(const char* texto) {
    if (!texto) return strdup("Enigma inválido");

    int len = strlen(texto);
    char* resultado = malloc(len + 1);
    if (!resultado) return strdup("Erro de memória");

    int j = 0;
    for (int i = 0; i < len;) {
        unsigned char c = texto[i];

        // Ignora caracteres de formatação Markdown (apenas ASCII)
        if (c == '*' || c == '#' || c == '_' || c == '`') {
            i++;  // Pula só esse byte
            continue;
        }

        // Copia bytes multibyte de UTF-8
        int bytes = 1;
        if ((c & 0x80) == 0x00) bytes = 1;
        else if ((c & 0xE0) == 0xC0) bytes = 2;
        else if ((c & 0xF0) == 0xE0) bytes = 3;
        else if ((c & 0xF8) == 0xF0) bytes = 4;

        for (int b = 0; b < bytes; b++) {
            resultado[j++] = texto[i++];
        }
    }

    resultado[j] = '\0';
    return resultado;
}




char* mostrarTelaNomeJogador(SDL_Renderer* renderer, TTF_Font* fonte) {
    int quit = 0;
    SDL_Event e;
    char nome[50] = "";
    int pos = 0;

    while (!quit) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) return NULL;

            if (e.type == SDL_TEXTINPUT && pos < 49) {
                strcat(nome, e.text.text);
                pos = strlen(nome);
            } else if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_BACKSPACE && pos > 0) {
                    nome[pos - 1] = '\0';
                    pos--;
                } else if (e.key.keysym.sym == SDLK_RETURN && pos > 0) {
                    quit = 1;
                }
            }
        }

        SDL_SetRenderDrawColor(renderer, 20, 20, 50, 255);
        SDL_RenderClear(renderer);

        SDL_Color corBranca = {255, 255, 255};

        renderTexto(renderer, fonte, "Digite seu nome:", 200, 100, corBranca);
        renderTexto(renderer, fonte, nome, 200, 150, corBranca);
        renderTexto(renderer, fonte, "(Pressione Enter para continuar)", 200, 220, corBranca);

        SDL_RenderPresent(renderer);
        SDL_Delay(5); // 60 FPS
    }

    char* resultado = malloc(strlen(nome) + 1);
    strcpy(resultado, nome);
    return resultado;
}
char* mostrarTelaDesafio(SDL_Renderer* renderer, TTF_Font* fonte, const char* enigma, int pontuacao) {

    SDL_Event e;
    char resposta[100] = "";
    int pos = 0;
    bool quit = false;

    while (!quit) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) return NULL;
            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_RETURN && pos > 0) {
                    quit = true;
                } else if (e.key.keysym.sym == SDLK_BACKSPACE && pos > 0) {
                    resposta[--pos] = '\0';
                } else if (e.key.keysym.sym == SDLK_ESCAPE) {
                    return NULL;
                }
            } else if (e.type == SDL_TEXTINPUT && pos < 99) {
                strcat(resposta, e.text.text);
                pos = strlen(resposta);
            }
        }

        SDL_SetRenderDrawColor(renderer, 20, 20, 50, 255);
        SDL_RenderClear(renderer);

        SDL_Color corBranca = {255, 255, 255};

        renderTexto(renderer, fonte, "Desafio do Mestre dos Números", 140, 30, corBranca);
        
        char* enigmaLimpo = removerMarkdown(enigma);
        if (enigmaLimpo) {
            renderTextoMultilinha(renderer, fonte, enigmaLimpo, 60, 100, corBranca);
            free(enigmaLimpo);
        }else{
            renderTexto(renderer, fonte, "Erro ao limpar enigma", 60, 100, corBranca);
        }
        
        renderTexto(renderer, fonte, "Sua resposta:", 60, 250, corBranca);
        renderTexto(renderer, fonte, resposta, 250, 250, corBranca);

        char buffer[50];
        sprintf(buffer, "Pontuacao: %d", pontuacao);
        renderTexto(renderer, fonte, buffer, 60, 400, corBranca);

        renderTexto(renderer, fonte, "[Enter] Enviar", 60, 320, corBranca);
        renderTexto(renderer, fonte, "[Esc] Voltar ao menu", 60, 360, corBranca);

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    char* final = malloc(strlen(resposta) + 1);
    strcpy(final, resposta);
    return final;
}

void jogarDesafio(SDL_Renderer* renderer, TTF_Font* fonte, const char* nome) {
    clear();
    printw("Olá, %s! Pressione qualquer tecla para continuar...\n", nome);
    getch();

    int pontuacao = 0;

    while (1) {
        clear();
        const char* promptDesafio = 
            "Gere apenas 1 enunciado de um enigma matemático simples e curto, ideal para crianças de até 10 anos. "
            "A resposta não deve aparecer. O enigma deve ter no máximo 150 caracteres. "
            "Além disso a resposta deve sempre ser um número, exemplo: quem sou eu, que dividido por mim sou eu mesmo? "
            "Retorne apenas o texto do enunciado, sem explicação, sem resposta, sem título.";
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
        
        char* resposta = mostrarTelaDesafio(renderer, fonte, desafio, pontuacao);
        if (!resposta) {
            free(desafio);
            break;
        }

        char respostaJogador[100] = "";
        strncpy(respostaJogador, resposta, sizeof(respostaJogador) - 1);
        respostaJogador[sizeof(respostaJogador) - 1] = '\0';
        free(resposta);

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
        snprintf(promptResposta, sizeof(promptResposta),
            "Resolva este enigma apenas com a resposta numerica, apenas a resposta em digito, sem explicar nada: %s", desafio);
        free(desafio);

        char* respostaIA = chamarIA(promptResposta);

        if (!respostaIA || strlen(respostaIA) < 1) {
            printw("Erro ao obter resposta da IA.\n");
            if (respostaIA) free(respostaIA);
            getch();
            return;
        }

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


void mostrarRankingSDL(SDL_Renderer* renderer, TTF_Font* fonte) {
    SDL_Event e;
    int quit = 0;

    while (!quit) {
        SDL_SetRenderDrawColor(renderer, 20, 20, 50, 255);
        SDL_RenderClear(renderer);

        SDL_Color corBranca = {255, 255, 255};

        renderTexto(renderer, fonte, "🏆 RANKING DOS JOGADORES", 140, 40, corBranca);

        if (!ranking) {
            renderTexto(renderer, fonte, "Nenhum jogador ainda.", 180, 120, corBranca);
        } else {
            Jogador* atual = ranking;
            int y = 100;
            int pos = 1;
            do {
                char linha[100];
                snprintf(linha, sizeof(linha), "%d. %s - %d pontos", pos++, atual->nome, atual->pontuacao);
                renderTexto(renderer, fonte, linha, 100, y += 40, corBranca);
                atual = atual->prox;
            } while (atual != ranking);
        }

        renderTexto(renderer, fonte, "[Esc] Voltar ao menu", 180, 420, corBranca);

        SDL_RenderPresent(renderer);

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                exit(0);
            }
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
                quit = 1;
            }
        }

        SDL_Delay(16);
    }
}


void mostrarMenuPrincipal(SDL_Renderer* renderer, TTF_Font* fonte) {
    int running = 1;
    SDL_Event e;

    SDL_Color corTexto = {255, 255, 255};
    SDL_Color corFundo = {30, 30, 60};
    SDL_Color corBotao = {70, 130, 180};

    SDL_Rect botoes[3] = {
        {200, 200, 200, 50},  // Jogar
        {200, 270, 200, 50},  // Ver Ranking
        {200, 340, 200, 50}   // Sair
    };

    const char* textos[3] = {"Jogar", "Ver Ranking", "Sair"};

    while (running) {
        SDL_SetRenderDrawColor(renderer, corFundo.r, corFundo.g, corFundo.b, 255);
        SDL_RenderClear(renderer);

        // Título
        SDL_Surface* tituloSurface = TTF_RenderUTF8_Blended(fonte, "Desafio do Mestre dos Números", corTexto);
        SDL_Texture* tituloTexture = SDL_CreateTextureFromSurface(renderer, tituloSurface);
        SDL_Rect tituloRect = {100, 100, tituloSurface->w, tituloSurface->h};
        SDL_RenderCopy(renderer, tituloTexture, NULL, &tituloRect);
        SDL_FreeSurface(tituloSurface);
        SDL_DestroyTexture(tituloTexture);

        for (int i = 0; i < 3; i++) {
            SDL_SetRenderDrawColor(renderer, corBotao.r, corBotao.g, corBotao.b, 255);
            SDL_RenderFillRect(renderer, &botoes[i]);

            SDL_Surface* textoSurface = TTF_RenderUTF8_Blended(fonte, textos[i], corTexto);
            SDL_Texture* textoTexture = SDL_CreateTextureFromSurface(renderer, textoSurface);
            SDL_Rect textoRect = {
                botoes[i].x + (botoes[i].w - textoSurface->w) / 2,
                botoes[i].y + (botoes[i].h - textoSurface->h) / 2,
                textoSurface->w,
                textoSurface->h
            };
            SDL_RenderCopy(renderer, textoTexture, NULL, &textoRect);
            SDL_FreeSurface(textoSurface);
            SDL_DestroyTexture(textoTexture);
        }

        SDL_RenderPresent(renderer);

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = 0;
            }
            if (e.type == SDL_MOUSEBUTTONDOWN) {
                int x = e.button.x;
                int y = e.button.y;
                for (int i = 0; i < 3; i++) {
                    if (x >= botoes[i].x && x <= botoes[i].x + botoes[i].w &&
                        y >= botoes[i].y && y <= botoes[i].y + botoes[i].h) {
                        
                        if (i == 0) {
                            // Jogar
                            char* nome = mostrarTelaNomeJogador(renderer, fonte);
                            if(nome){
                               jogarDesafio(renderer, fonte, nome);
                                free(nome);
                            }
                        } else if (i == 1) {
                            mostrarRankingSDL(renderer, fonte);  // função futura
                        } else if (i == 2) {
                            running = 0;  // Sair
                        }
                    }
                }
            }
        }
    }
}



int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0 || TTF_Init() != 0) {
        printf("Erro ao inicializar SDL ou SDL_ttf: %s\n", SDL_GetError());
        return 1;
    }
    
    SDL_StartTextInput();

    SDL_Window* window = SDL_CreateWindow("Desafio do Mestre dos Números",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 600, 500, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    TTF_Font* fonte = TTF_OpenFont("Fonte.ttf", 24);  // ou substitua pela sua fonte

    if (!fonte) {
        printf("Erro ao carregar fonte.\n");
        return 1;
    }

    mostrarMenuPrincipal(renderer, fonte);  // 👈 Mostra o menu
    
    TTF_CloseFont(fonte);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    
    SDL_StopTextInput();
    
    TTF_Quit();
    SDL_Quit();
    return 0;
}


