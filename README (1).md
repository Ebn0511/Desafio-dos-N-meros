# 🎮 Desafio do Mestre dos Números

**Desafio do Mestre dos Números** é um jogo educativo e interativo desenvolvido para crianças de até 10 anos, com o objetivo de estimular o raciocínio lógico e matemático de forma divertida e intuitiva.  

Utilizando uma interface gráfica com a biblioteca **SDL2**, o jogador é desafiado a resolver enigmas matemáticos simples, gerados dinamicamente por uma Inteligência Artificial conectada via API.  

Ao acertar os enigmas, o jogador acumula pontos e entra para o **ranking** dos melhores participantes!

---

## 👨‍💻 Desenvolvedores

Este jogo foi desenvolvido por:

- **Enzo Nunes**  
- **Kauã Novello**  
- **Davi Mauricio**

---

## ⚙️ Tecnologias Utilizadas

- Linguagem C  
- SDL2 e SDL2_ttf (interface gráfica)  
- ncurses (feedback via terminal)  
- cURL (requisições HTTP)  
- cJSON (parser de JSON)  
- API da OpenRouter (DeepSeek Prover) para geração de enigmas e respostas

---

## 🧩 Funcionalidades

- Geração automática de enigmas simples e curtos (resposta sempre numérica)
- Interface gráfica amigável para digitação de nome e resposta
- Verificação de respostas com feedback instantâneo
- Ranking com os jogadores e suas pontuações
- Prevenção de enigmas repetidos
- Suporte a acentos e caracteres especiais

---

## 💻 Como Compilar

Antes de compilar, certifique-se de que as bibliotecas **SDL2**, **SDL2_ttf**, **ncurses**, **libcurl** e **cJSON** estão instaladas em sua máquina.

### Comando para compilar:

```bash
gcc jogo.c gemini.c cJSON.c -o jogo -lSDL2 -lSDL2_ttf -lcurl -lncurses
```

---

## ▶️ Como Executar

Após a compilação, execute o programa com:

```bash
./jogo
```

---

## 📝 Observações

- Certifique-se de que o arquivo de fonte `Fonte.ttf` esteja no mesmo diretório do executável.
- O jogo requer conexão com a internet para buscar enigmas e respostas usando a API.
- O limite de requisições gratuitas pode ser atingido dependendo do plano da API usada.