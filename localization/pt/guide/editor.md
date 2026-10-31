# O editor

O editor mostra seu preset como um grafo de nós. O áudio flui das saídas de cada nó, na parte inferior do nó, para as entradas do nó seguinte, na parte superior. Para abri-lo, selecione Show Editor no menu do Curve. Para fechá-lo, clique no botão de fechar ou selecione Hide Editor. O áudio continua funcionando em ambos os casos.

A barra de status na parte inferior do editor mostra dicas e descreve o que estiver sob o ponteiro.

## Adicionar um nó

Clique com o botão direito no fundo do editor e escolha entre:

- **Audio I/O:** nós de entrada de áudio, saída de áudio e Interface Loopback (In).
- **MIDI I/O:** nós de entrada e saída MIDI.
- **Curve Plugins:** [os plug-ins integrados do Curve](curve-plugins.md).
- **Seus plug-ins verificados:** listados abaixo desses menus, organizados por fabricante.

Você também pode arrastar um arquivo de plug-in, ou um [arquivo de EQ](eq-and-autoeq.md), para a janela do editor.

## Conectar nós

Arraste de um pino de um nó até um pino de outro. Os pinos de áudio são verdes e os pinos MIDI são vermelhos. Você pode conectar uma saída a várias entradas e montar a cadeia que quiser.

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
| Configure Audio I/O | Altera o número de pinos de entrada e saída de um plug-in. |
| Audio Settings... | Aparece nos nós de entrada e saída de áudio. Abre Audio Settings. |
| Save Plug-in State | Salva os ajustes do plug-in em um arquivo. |
| Load Plug-in State | Carrega os ajustes do plug-in de um arquivo. |

## Alterar o número de canais de um plug-in

Se um plug-in não tiver o número de pinos desejado, clique com o botão direito no nó dele e selecione Configure Audio I/O.
