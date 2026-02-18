# Mandelbrot Renderer

Um renderizador gráfico C11 minimalista e portável para jogos, simulações e programação criativa. Inspirado em jogos como Doom e Quake, segue a arquitetura clássica de Handmade Hero, separando a camada de plataforma (Win32/X11) da camada de aplicação. O exemplo é uma implementação relativamente interativa do [Conjunto de Mandelbrot](https://pt.wikipedia.org/wiki/Conjunto_de_Mandelbrot) feita com procesamento paralelo e vetorização SIMD. Ainda podem ser feitas muitas melhorias e extensões na parte da camada de plataforma.

## Compilar e executar

No Windows:
```bash
make
mandelbrot-renderer.exe
```

No Linux:
```bash
make
./mandelbrot-renderer
```

**Observação:** No Linux, a camada de plataforma exige as bibliotecas de desenvolvimento do X11 para compilar corretamente.

## Como interagir com o programa

Para o usuário, é possível navegar na tela pelas setinhas e alterar o zoom pelas teclas '+' e '-'.

## Características da camada de plataforma

- **Fundação** — É um *boilerplate* limpo que pode ser reaproveitado
- **C puro** — Sem C++, sem dependências pesadas (apenas a biblioteca padrão do C)
- **Multiplataforma** — Windows (Win32) e Linux (X11); facilmente expansível para macOS, web, etc
- **API simples** — Implementa apenas um callback: `UpdateAndRender(Input*, OffscreenBuffer*)`
- **Frame timing** — Cálculo automático do delta-time calculation a cada frame
- **Full input** — Teclado (A-Z, 0-9, setinhas, modificadores), mouse (posição, 3 botões, scroll)
- **E/S de arquivos** — Abstração para carregamento de arquivos independente do SO