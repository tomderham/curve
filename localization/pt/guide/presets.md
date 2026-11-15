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

Se vários presets forem iguais exceto por algumas configurações, como um nó em bypass ou um plug-in configurado de outro jeito, você pode transformá-los em cenas. As cenas compartilham seus plug-ins e conexões, então basta fazer uma alteração uma vez para atualizar todas. Cada cena fica no menu do Curve como qualquer outro preset, e você muda para ela do mesmo jeito. Na janela Save Preset, cenas que compartilham entre si têm a mesma cor, e selecionar uma destaca as outras.

O que cada cena guarda para si:

- Se cada nó está em bypass.
- As configurações dos nós definidos como Vary by Scene. Clique com o botão direito em um nó e selecione Vary by Scene; ele mostra um pequeno selo de cartões empilhados.

Todo o resto é compartilhado: os outros nós e suas configurações, as conexões, o roteamento de canais e os mapeamentos MIDI. Altere um nó compartilhado em qualquer cena, salve, e todas as cenas recebem a alteração. Um nó definido como Vary by Scene continua compartilhando seus canais e conexões.

Para criar uma cena:

1. Carregue o preset do qual a nova cena deve partir.
2. Para cada plug-in que deve ser configurado de outro jeito na nova cena, clique com o botão direito nele e selecione Vary by Scene.
3. Faça as alterações para a nova cena.
4. Selecione Save As Preset..., digite um novo nome, escolha a opção de cena e clique em Save.

Antes de salvar, a janela Save Preset informa quais outras cenas o salvamento atualiza e cita os nós compartilhados cujas configurações vão mudar nelas. Se uma configuração for só para esta cena, cancele, defina esse nó como Vary by Scene e salve de novo.

Para alterar uma cena, carregue-a, faça as alterações e salve com o mesmo nome. Salvar por cima de outra cena com a qual ela compartilha substitui essa cena, que continua sendo uma cena. Um nó que você adiciona aparece também nas outras cenas, em bypass, para que soem iguais até você ativá-lo nelas.

Para uma cena deixar de compartilhar com as outras, clique com o botão direito nela na janela Save Preset e selecione Convert to Standalone Preset. Ela mantém as configurações atuais.

## Renomear ou apagar um preset

Na janela Save Preset, clique com o botão direito em um preset para renomeá-lo, movê-lo para o Lixo ou mostrá-lo no Finder.

Os presets são arquivos `.filtergraph` em `~/Library/Application Support/Curve/Presets/`, portanto você também pode gerenciá-los no Finder.
