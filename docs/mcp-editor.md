# MCP local do Editor

O Frigga Editor expõe um endpoint MCP local para permitir que o Cursor
inspecione e ajuste o projeto através de ferramentas controladas. O desenho é
local-only: o Cursor inicia o bridge via stdio e o bridge conversa com uma
instância do Editor por um socket TCP restrito a `127.0.0.1`.

## Uso

1. Compile o Editor.
2. Configure o MCP do workspace com `.cursor/mcp.json` (uma vez).
3. Inicie `Editor` e abra um projeto.
4. Use as ferramentas MCP; o bridge sobe sem o Editor e conecta sob demanda.

O Editor grava temporariamente o endpoint autenticado em
`/tmp/frigga-editor-mcp.endpoint`. O bridge lê o arquivo na primeira tool call
(e de novo após uma queda de conexão) e usa o token de sessão gerado pelo
Editor. O arquivo é removido ao encerrar o Editor.

Fechar e reabrir o Editor **não** exige reiniciar o MCP no Cursor: o bridge
reconecta automaticamente no próximo `tools/call`. Só é preciso reiniciar o
servidor MCP se o processo do bridge cair ou se `.cursor/mcp.json` mudar.

No Windows, o mesmo protocolo usa loopback TCP; o caminho do endpoint segue a
área temporária do sistema.

## Ferramentas disponíveis

- `project.inspect`
- `scene.inspect`
- `scene.open`
- `scene.create`
- `scene.replace_snapshot`
- `scene.save`
- `assets.list`
- `assets.validate`
- `assets.cook`
- `runtime.start`, `runtime.stop`, `runtime.status`
- `logs.recent`
- `editor.invoke` com ações allowlisted (`save_scene`, `play`, `stop`,
  `validate_assets`)

Operações de edição são executadas na thread principal do Editor no ponto
seguro do frame. O bridge não executa shell, não grava diretamente arquivos
do projeto e não aceita caminhos fora da raiz do projeto. `scene.create` e
`assets.cook` aceitam `dry_run`.

## Transporte futuro

As ferramentas não dependem de stdio. O bridge possui os contratos
`McpTransport` e `StreamableHttpMcpTransport`, e o dispatcher é separado do
transporte. Uma implementação futura de Streamable HTTP poderá substituir o
transporte sem duplicar handlers, validações ou integração com
`EditorControlService`.

O Streamable HTTP não está habilitado nesta versão; não há porta de rede
externamente exposta.

## Diagnóstico

Falhas de conexão normalmente indicam que o Editor ainda não foi iniciado, que
o endpoint expirou ou que outra instância substituiu o endpoint temporário.
Com o Editor fechado, as tool calls falham com erro claro; ao reabrir o Editor,
a próxima chamada reconecta sem reload do MCP. Consulte `frigga.log` e
`frigga-crash.log` para diagnóstico do Editor.
