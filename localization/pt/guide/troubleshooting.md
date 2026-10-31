# Solução de problemas

## Não há som ou alguns canais estão mudos

- Verifique se os nós estão conectados até Audio Output.
- Se o nó Interface Loopback (In) estiver em um preset mas você não o estiver usando, remova-o ou deixe os pinos dele desconectados. Consulte [Loopback](loopback.md).
- Se você desconectou a interface selecionada, o Curve fica em silêncio até ela voltar, em vez de usar outra saída. Para usar outra, selecione-a em Audio Settings.
- Verifique se nenhum nó está em bypass ou mudo e se os canais certos estão ativados em [Audio Settings](audio-settings.md).

## Curve Help informa que o conteúdo não está disponível

Curve Help só abre quando o Curve está na pasta Aplicativos. Mova-o para lá e tente novamente.

## Não encontro o Curve depois de abri-lo

O Curve fica na barra de menus e normalmente não abre nenhuma janela. Clique no ícone dele na barra de menus e selecione Show Editor. Para mostrá-lo também no Dock, ative Settings > Show in Dock.

## Minhas alterações em um preset sumiram

O Curve não salva alterações automaticamente. Use Save As Preset... e substitua o preset existente. Consulte [Presets](presets.md).

## Um plug-in não aparece no menu

Selecione Settings > Plug-in Manager e execute a verificação para o formato desse plug-in. Consulte [Primeiros passos](getting-started.md).

## Um plug-in tem o número errado de pinos

Clique com o botão direito no nó dele e selecione Configure Audio I/O.

## A taxa de amostragem fica mudando

Outro app pode estar alterando a taxa de amostragem do hardware. Para que o Curve force o retorno, ative Always enforce sample rate em [Audio Settings](audio-settings.md).

## Os AirPods não trocam automaticamente

Ative Settings > Release audio interface when idle. Consulte [Ajustes de áudio e dispositivos](audio-settings.md).

## O Curve não responde ao meu controlador MIDI

Verifique se o controlador está ativado nas entradas MIDI em Audio Settings e se o mapeamento foi salvo com o preset. Consulte [Controle MIDI](midi-control.md).
