# O editor

O editor mostra seu preset como um grafo de nós. O áudio flui das saídas de cada nó, na parte inferior do nó, para as entradas do nó seguinte, na parte superior. Para abri-lo, selecione Show Editor no menu do Curve. Para fechá-lo, clique no botão de fechar ou selecione Hide Editor. O áudio continua funcionando em ambos os casos.

A barra de status na parte inferior do editor mostra dicas e descreve o que estiver sob o ponteiro. Para um plug-in, ela também mostra o fabricante e o formato, por exemplo AudioUnit ou VST3.

## Adicionar um nó

Clique com o botão direito no fundo do editor e escolha entre:

- **Audio I/O:** nós de entrada de áudio, saída de áudio e Interface Loopback (In).
- **MIDI I/O:** nós de entrada e saída MIDI.
- **Curve Plugins:** [os plug-ins integrados do Curve](curve-plugins.md).
- **Seus plug-ins verificados:** listados abaixo desses menus, organizados por fabricante.

Você também pode arrastar um arquivo de plug-in, ou um [arquivo de EQ](eq-and-autoeq.md), para a janela do editor.

## Conectar nós

Arraste de um pino de um nó até um pino de outro. Os pinos de áudio são cinza e os pinos MIDI são âmbar. Você pode conectar uma saída a várias entradas e montar a cadeia que quiser.

Canais consecutivos entre os mesmos dois nós, como um par estéreo, são desenhados como uma única linha mais grossa que se divide até cada pino. Os canais continuam separados e nada é mixado. Arraste a linha para reconectar um canal de cada vez.

## Organizar os nós

Arraste um nó para movê-lo. Os nós se ajustam a uma grade, de modo que nós alinhados têm conexões retas. Segure a tecla Option enquanto arrasta para posicionar um nó livremente. Arrastá-lo de novo sem Option o devolve à grade.

Para organizar o grafo inteiro pelo fluxo do sinal, clique com o botão direito no fundo e selecione Tidy Up Layout. Se o grafo precisar de mais espaço, a janela cresce para acomodá-lo, até o tamanho da sua tela. Depois, Undo Tidy Up restaura seu layout e o tamanho da janela, até você mover um nó novamente.

## Ignorar um nó

Clique no botão liga/desliga do nó. Clique novamente para reativá-lo. Você também pode clicar com o botão direito no nó e selecionar Toggle Bypass.

## Abrir a janela de um plug-in

Clique duas vezes no nó.

Para que a janela abra sempre que o preset for carregado, mesmo com o editor fechado, clique com o botão direito no nó, selecione Always Open Plug-in GUI with Preset e use Save As Preset... A janela abre na posição em que estava ao salvar.

## Menu do nó

Clique com o botão direito em um nó para ver estas opções:

| Item | Função |
|---|---|
| Delete This Node | Remove o nó. |
| Disconnect All Pins | Remove todas as conexões do nó. |
| Toggle Bypass | Ativa ou desativa o bypass. |
| Rename This Node | Altera o nome mostrado no nó. |
| MIDI Learn | Inicia o [MIDI Learn](midi-control.md). |
| Clear MIDI Mappings | Remove os mapeamentos MIDI do nó. Só aparece se ele tiver algum. |
| Always Open Plug-in GUI with Preset | Abre a janela do plug-in sempre que o preset é carregado. |
| Import EQ from AutoEQ... | Aparece nos nós Parametric EQ. Consulte [EQ para fones de ouvido e AutoEQ](eq-and-autoeq.md). |
| Import EQ from file... | Aparece nos nós Parametric EQ. Consulte [EQ para fones de ouvido e AutoEQ](eq-and-autoeq.md). |
| Configure Audio I/O | Altera os layouts de canais de um plug-in e desativa ou oculta os pinos dele. Veja abaixo. |
| Audio Settings... | Aparece nos nós de entrada e saída de áudio. Abre Audio Settings. |
| Save Plug-in State | Salva os ajustes do plug-in em um arquivo. |
| Load Plug-in State | Carrega os ajustes do plug-in de um arquivo. |

## Configurar as entradas e saídas de um plug-in

Clique com o botão direito no nó de um plug-in e selecione Configure Audio I/O. Os pinos de um plug-in são agrupados em buses, como uma entrada estéreo principal e uma entrada de sidechain. Selecione um bus pelo número e use:

- **Channel Layout:** define quantos canais o bus tem. Layouts com mais de dois canais são agrupados por número de canais. Um plug-in surround ou ambissônico pode tratar de forma diferente dois layouts com o mesmo número de canais, então escolha o que corresponde ao seu áudio.
- **Enabled:** desativa o bus, o que remove os pinos dele. Fica esmaecido se o plug-in não puder funcionar sem o bus.
- **Shown:** oculta os pinos do bus no grafo, mas o bus continua funcionando. Use para tirar do caminho um bus que você não usa e que não pode ser desativado. Ocultar um bus remove as conexões dele, e uma entrada oculta recebe silêncio. Shown fica esmaecido enquanto o bus está desativado.

Se o plug-in tiver um pino MIDI, MIDI Port tem a própria configuração Shown. Um lado que só tem MIDI, como a entrada de um instrumento, tem apenas essa configuração.

Alterar um layout ou ativar ou desativar um bus desconecta todos os pinos do nó. As configurações são salvas com o preset. Ao apontar para um nó com pinos ocultos, a barra de status indica isso.
