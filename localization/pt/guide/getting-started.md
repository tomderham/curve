# Primeiros passos com o Curve

O Curve requer macOS Sonoma 14.2 ou posterior, em Apple silicon ou Intel.

## Instalar o Curve

1. Baixe o Curve na [versão mais recente](https://github.com/tomderham/curve/releases).
2. Copie `Curve.app` para a pasta Aplicativos e abra-o.

O Curve fica na barra de menus, portanto, por padrão, seu ícone aparece ali e não no Dock. A janela do editor abre no primeiro uso. Depois disso, o Curve inicia sem nenhuma janela aberta.

Para acessar o menu do Curve, clique no ícone da barra de menus ou no ícone de menu no canto superior esquerdo da janela do editor.

## Procurar plug-ins

Pule esta etapa se você quiser usar apenas os plug-ins integrados do macOS, como o AUNBandEQ, ou [os plug-ins do próprio Curve](curve-plugins.md).

1. Selecione Settings > Plug-in Manager.
2. Clique em Options... no canto inferior esquerdo e selecione 'Scan for new or updated AudioUnit plug-ins'. Isso pode demorar um pouco, e o macOS pode pedir que você permita que o Curve acesse pastas usadas por plug-ins.
3. Se você usa plug-ins VST3, repita com 'Scan for new or updated VST3 plug-ins'.
4. Feche a janela.

## Escolher seus dispositivos de áudio

1. Selecione Settings > Audio Settings.
2. Selecione a interface de saída e, a menos que esteja usando [loopback](loopback.md), a interface de entrada.
3. Selecione os canais desejados, além da taxa de amostragem e do tamanho do buffer.
4. Se você usa MIDI, selecione suas interfaces MIDI.
5. Feche a janela.

## Criar seu primeiro preset

Um preset é uma cadeia de nós conectados no editor. Se o editor não estiver aberto, selecione Show Editor no menu do Curve.

1. Você verá os nós Audio Input, Interface Loopback (In) e Audio Output, com pinos verdes para os canais ativados.
2. Clique com o botão direito no fundo do editor e selecione um plug-in. O nó dele aparece com as entradas em cima e as saídas embaixo.
3. Arraste de um pino verde do nó de entrada até um pino de entrada do plug-in.
4. Arraste dos pinos de saída do plug-in até o nó Audio Output.
5. Clique duas vezes no nó do plug-in para abrir a janela dele e ajustá-lo.
6. Selecione Save As Preset... no menu do Curve e dê um nome ao preset.

Se um preset não usar o nó Interface Loopback (In), remova-o ou, pelo menos, deixe os pinos dele desconectados. Caso contrário, ele pode silenciar seu áudio.

Tenha cuidado com roteamentos incomuns. O Curve tem alguma proteção contra loops de realimentação, mas uma configuração ruim ainda pode danificar equipamentos ou sua audição.

## Concluir a configuração

- Para iniciar o Curve ao fazer login, selecione Settings > Open at Login.
- As verificações automáticas de atualização vêm ativadas por padrão. Você pode alterar isso em Settings > Auto-Check for App Updates.

## Próximos passos

- [Presets](presets.md)
- [EQ para fones de ouvido e AutoEQ](eq-and-autoeq.md)
- [Loopback e áudio do sistema](loopback.md)
