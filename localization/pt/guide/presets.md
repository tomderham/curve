# Presets

Um preset armazena um grafo inteiro: seus plug-ins e os ajustes deles, conexões, roteamento de canais de áudio e mapeamentos MIDI. Alternar presets permite mudar os ajustes dos mesmos plug-ins, trocar por outros plug-ins ou alterar o roteamento de canais.

## Salvar um preset

1. Selecione Save As Preset... no menu do Curve.
2. Digite um nome e clique em Save.

As alterações não são salvas automaticamente. Use Save As Preset... sempre que quiser mantê-las.

## Alterar um preset existente

1. Carregue o preset selecionando-o no menu do Curve.
2. Faça suas alterações.
3. Selecione Save As Preset..., escolha o nome existente e confirme que deseja substituí-lo.

## Alternar presets

Seus presets aparecem no menu do Curve em ordem alfabética, e o atual tem uma marca de seleção. Selecione um para passar a ele imediatamente. O Curve não pergunta antes, então salve as alterações que quiser manter antes de alternar.

Você também pode alternar presets [pelo teclado](keyboard-shortcuts.md) ou [a partir de um controlador MIDI](midi-control.md). Para carregar diretamente um arquivo de preset, arraste-o para a janela do editor.

## Começar um preset novo

Para recomeçar com um grafo vazio, clique no ícone de menu no canto superior esquerdo da janela do editor e selecione New Preset. Todos os nós são removidos, exceto Audio Input, Interface Loopback (In) e Audio Output, que ficam desconectados, então não há som até você conectá-los.

A menos que o grafo tenha só esses nós, o Curve pergunta antes, porque as configurações alteradas na janela de um plug-in não são registradas como alterações. Selecione Save As Preset... para manter a configuração atual, ou Discard and Start New.

## Cenas

Se vários presets têm partes em comum, você pode transformá-los em cenas. Por exemplo, as cenas podem compartilhar a mesma correção de alto-falantes mas usar instrumentos ou caminhos de sinal diferentes, ou usar os mesmos instrumentos com correções de alto-falantes diferentes. As cenas compartilham seus plug-ins e conexões, então você só precisa fazer uma alteração uma vez para atualizar todas. Cada cena continua no menu do Curve como qualquer outro preset, e você troca para ela da mesma forma.

As cenas que compartilham entre si são marcadas com a mesma cor no menu do Curve e na janela Save Preset.

Trocar para outra cena é rápido: o Curve muda só o que essa cena tem de próprio, sem carregar seus plug-ins de novo. Se você mudou um ajuste compartilhado desde que carregou, ou se falta um plug-in, a cena é carregada por completo, e a alteração se perde, como acontece ao trocar de preset.

O que cada cena guarda para si:

- Se cada nó está em bypass ou mudo. Um nó em bypass deixa sua entrada passar sem alteração. Um nó mudo não deixa passar nada, então corta o caminho em que está. Consulte [Silenciar um nó](editor.md).
- Os ajustes dos nós definidos como Vary by Scene. Clique com o botão direito em um nó e selecione Vary by Scene; ele mostra um pequeno selo de cartões empilhados.
- Quais janelas de plug-ins abrem com ela (Always Open Plug-in GUI with Preset), e onde ficam. Um nó que você adiciona a uma cena não abre com as outras.

Todo o resto é compartilhado: os outros nós e seus ajustes, as conexões, o roteamento de canais e os mapeamentos MIDI. Altere um nó compartilhado em qualquer cena, salve, e todas as cenas recebem a alteração. Um nó definido como Vary by Scene continua compartilhando seus canais e conexões.

Para criar uma cena:

1. Carregue o preset do qual a nova cena deve partir.
2. Para cada plug-in que deve ser ajustado de outra forma na nova cena, clique nele com o botão direito e selecione Vary by Scene.
3. Faça as alterações para a nova cena. Para deixar um caminho de fora, silencie um nó dele.
4. Selecione Save As Preset..., digite um novo nome, escolha a opção de cena e clique em Save.

Antes de salvar, a janela Save Preset mostra quais outras cenas serão atualizadas e cita os nós compartilhados cujos ajustes mudarão nelas. Se uma alteração é só para esta cena, cancele, clique com o botão direito no nó, selecione Vary by Scene e salve de novo. Assim, cada cena mantém seus próprios ajustes para esse nó. Ela continua sendo uma cena, e as cenas continuam compartilhando todo o resto.

Para alterar uma cena, carregue-a, faça as alterações e salve-a com o mesmo nome. Salvar por cima de outra cena com a qual ela compartilha substitui essa cena, que continua sendo uma cena. Um nó que você adiciona também aparece nas outras cenas, em bypass, para que soem igual até você ativá-lo nelas. Um nó em bypass ainda deixa sua entrada passar, então, para que um caminho novo fique em silêncio nas outras cenas, silencie nelas um dos nós dele.

Para que uma cena deixe de compartilhar com as outras, clique nela com o botão direito na janela Save Preset e selecione Convert to Standalone Preset. Ela mantém seus ajustes atuais.

## Renomear ou apagar um preset

Na janela Save Preset, clique com o botão direito em um preset para renomeá-lo, movê-lo para o Lixo ou mostrá-lo no Finder.

Os presets são arquivos `.filtergraph` em `~/Library/Application Support/Curve/Presets/`, portanto você também pode gerenciá-los no Finder.
