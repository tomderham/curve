# Solução de problemas

## Não há som ou alguns canais estão mudos

- Verifique se os nós estão conectados até Audio Output.
- Se o nó Interface Loopback (In) estiver em um preset mas você não o estiver usando, remova-o ou deixe os pinos dele desconectados. Consulte [Loopback](loopback.md).
- Se você desconectou a interface selecionada, o Curve fica em silêncio até ela voltar, em vez de usar outra saída. Para usar outra, selecione-a em Audio Settings.
- Verifique se nenhum nó está em bypass ou mudo e se os canais certos estão ativados em [Audio Settings](audio-settings.md).

## Curve Help informa que o conteúdo não está disponível

Curve Help só abre quando o Curve está na pasta Aplicativos. Mova-o para lá e tente novamente.

## Não encontro o Curve depois de abri-lo

O Curve fica na barra de menus e normalmente não abre nenhuma janela. Clique no ícone dele na barra de menus e selecione Show Editor. Para mostrá-lo também no Dock, selecione Settings > Preferences... e ative Show in Dock.

## Minhas alterações em um preset sumiram

O Curve não salva alterações automaticamente. Use Save As Preset... e substitua o preset existente. Consulte [Presets](presets.md).

## Um plug-in não aparece no menu

Selecione Settings > Plug-in Manager e execute a verificação para o formato desse plug-in. Consulte [Primeiros passos](getting-started.md).

Se ele aparecer em vermelho no fim da lista do Plug-in Manager, com Scan failed como categoria, o plug-in travou ou levou mais de 30 segundos para carregar, e as verificações agora o ignoram. Clique nele com o botão direito e selecione Retry Scan, ou selecione Remove from List para removê-lo.

## Um plug-in tem o número errado de pinos

Clique com o botão direito no nó dele e selecione Configure Audio I/O.

## A taxa de amostragem fica mudando

Outro app pode estar alterando a taxa de amostragem do hardware. Para que o Curve force o retorno, ative Always enforce sample rate em [Audio Settings](audio-settings.md).

## Os AirPods não trocam automaticamente

Selecione Settings > Preferences... e ative Release audio interface when idle. Consulte [Ajustes de áudio e dispositivos](audio-settings.md).

## As teclas de volume não mudam o volume do Curve

- Verifique se Volume Keys, na aba Keyboard de Settings > Preferences..., não está em Off.
- Se a aba Keyboard mostrar Allow Accessibility Access..., clique nessa opção e ative o Curve em Acessibilidade. Se o Curve já estiver ativado lá, desative e ative de novo.
- Com When macOS Can't Control the Interface's Volume, o macOS fica com as teclas se puder ajustar o volume dos canais pelos quais o Curve toca, ou se a saída do macOS não for a interface de saída do Curve. Ative Force macOS system output to this interface em Audio Settings ou selecione Always. Consulte [Atalhos de teclado](keyboard-shortcuts.md).
- Com Control Interface Loopback Volume, o Curve só fica com as teclas enquanto o preset tiver um nó Interface Loopback (In) conectado. Consulte [Loopback](loopback.md).

## O Curve não responde ao meu controlador MIDI

Verifique se o controlador está ativado nas entradas MIDI em Audio Settings e se o mapeamento foi salvo com o preset. Consulte [Controle MIDI](midi-control.md).
