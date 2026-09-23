# MCP de documentação (Freyr / Freya / Skirnir)

O `frigga-docs` expõe a documentação markdown das três bibliotecas como
ferramentas MCP, **lendo direto do GitHub na versão pinada** — não de
`build/_deps` e não do HTML publicado no GitHub Pages. Isso faz o mesmo
servidor funcionar para quem tem o engine compilado e para usuário final que
nunca clonou nada: só precisa de rede.

Registrado em `.mcp.json` ao lado do `frigga-editor`, iniciado por stdio pelo
cliente. Não exige o Editor em execução.

## Por que git (e não `build/_deps`)

- `build/_deps/*-src` só existe em quem configurou o build do engine, e some
  num reconfigure limpo.
- `main` no GitHub nem sempre é o que está compilado (pin no `CMakeLists.txt`).
- Git entrega as duas coisas: ref explícita e reprodutível (`Freyr@v0.39.6`
  devolve sempre o mesmo conteúdo) e alcance para usuário final.

Descartados de propósito: tarball do repositório (o Freya tem 81 MB para 80 KB
de docs), scraping do Pages (HTML convertido, sem número de linha) e RAG com
embeddings (46 arquivos, ~265 KB — busca lexical resolve).

## Resolução de versão

A ref de cada biblioteca é resolvida nesta ordem, e a origem é reportada em
`ref_source`:

| Prioridade | Fonte | `ref_source` |
| --- | --- | --- |
| 1 | `--ref freyr=<ref>` na linha de comando | `argument` |
| 2 | `FRIGGA_SDK_DEPS` em `Sdk/FriggaSdkConfig.cmake` (ou `build/Sdk/…`) | `sdk` |
| 3 | `GIT_TAG` do `FetchContent_Declare` no `CMakeLists.txt` | `cmake` |
| 4 | `main` | `default` |

`FRIGGA_SDK_DEPS` é gravado por `cmake/PackFriggaSdk.cmake` a partir das
variáveis `FRIGGA_{SKIRNIR,FREYR,FREYA}_TAG` do `CMakeLists.txt` — ou seja, a
mesma linha que pinia o FetchContent alimenta a documentação. Referências
`${VAR}` de tag são expandidas pelo servidor; se não resolverem, ele cai para a
próxima fonte em vez de usar lixo.

## Ferramentas

- `docs.catalog` — bibliotecas, ref servida, páginas e títulos. `sections: true`
  devolve os headings com `slug` e `line` de cada página.
- `docs.search` — busca BM25 por seção em todas as bibliotecas (ou `library`).
  Devolve `path`, `section`, `line`, `snippet` e `cite`
  (`freyr@v0.39.6:docs/api/query.md:14`).
- `docs.read` — página inteira ou uma seção, em markdown, paginado por linha
  (`offset`/`limit`). `path` aceita `freyr/docs/api/query.md`,
  `freyr:docs/api/query.md`, ou sufixo único como `api/query.md`; caminho
  ambíguo ou com `..` vira erro explícito com as alternativas.
- `docs.refresh` — relista a biblioteca no GitHub (1 chamada de API) e descarta
  a árvore em cache.

Uma busca devolve `cite` pronto para citar; `docs.read` com `section` devolve
`line_start`/`cite` apontando para o heading — os números correspondem ao `.md`
no repositório, então dá para abrir o arquivo real naquele ponto.

## Cache e cota de API

- `git/trees/<ref>?recursive=1` → **1 chamada de API por biblioteca**; `truncated`
  é rejeitado em vez de indexar pela metade.
- `raw.githubusercontent.com/...` → um pedido por arquivo, **não conta** na cota
  da API (CDN separado). Os blobs são cacheados em disco **por SHA do objeto**,
  então releitura não custa rede alguma.
- Árvores cacheadas revalidam após `--ttl` (300 s por padrão). Se a API estourar
  (403/429, limite de 60/h sem token), a árvore antiga é servida com
  `"stale": true`; sem cache nenhum, o erro `rate_limited` é devolvido com a
  explicação.
- Cache por usuário em `%LOCALAPPDATA%/frigga-docs` (Windows) ou
  `~/.cache/frigga-docs`. Apagar a pasta é sempre seguro.

Para a cota diária, um token (`GITHUB_TOKEN` via cabeçalho) ainda não é lido —
é a extensão natural se o volume crescer, junto com o modo servidor da fase 3.

## Uso

```bash
python3 tools/docs-mcp/server.py                    # stdio, padrão
python3 tools/docs-mcp/server.py --ref freyr=v0.39.6 --ttl 60
python3 tools/docs-mcp/server.py --repo freyr=meu-fork/Freyr --root .
```

Somente biblioteca especificada: `--root` aponta para a pasta com
`CMakeLists.txt`/`Sdk/` usada na resolução de pin (padrão: diretório atual).

## Limitações conhecidas

- Deps com override local (`FETCHCONTENT_SOURCE_DIR_*`, como o `C:/dev/Freya`
  deste checkout) não são alcançáveis por git remoto: a ref servida é o **pin
  publicado**, não a cópia de trabalho local (que pode ter edits em
  `docs/*.md` não commitados). Use `--ref`/`--repo` para um fork/branch.
- `main` como fallback pode descrever API que o projeto ainda não pinia; olhe o
  `ref_source` antes de confiar.
- O texto vem do markdown do repositório, não do HTML do Pages — o índice
  `search.json`/`search_index.json` publicado não é usado hoje.

## Relacionado

`docs/mcp-editor.md` — bridge do Editor, compartilha `tools/frigga-mcp/transports.py`
com este servidor (os dois diretórios precisam continuar irmãos, inclusive em
`Sdk/tools/`).
