# Ajustes de áudio e dispositivos

Selecione Settings > Audio Settings para escolher seus dispositivos de áudio e MIDI.

## Escolher interfaces e canais

Selecione uma interface de saída e, se o seu preset usar uma, uma interface de entrada. Depois ative os canais desejados. Eles aparecem como pinos verdes nos nós Audio Input e Audio Output.

Se você usa o [nó Interface Loopback (In)](loopback.md) do Curve, não precisa de uma interface de entrada.

## Taxa de amostragem e tamanho do buffer

Selecione aqui a taxa de amostragem e o tamanho do buffer. Um buffer menor significa menos latência, mas exige mais do seu Mac e pode causar falhas no áudio.

**Always enforce sample rate** vem desativado por padrão. Quando desativado, o Curve se adapta se a interface de áudio ou outro app alterar a taxa de amostragem do hardware. Quando ativado, o Curve força o retorno à taxa que você escolheu.

## Dispositivos MIDI

Ative as entradas e a saída MIDI que deseja usar. Use um nó MIDI Input ou MIDI Output para conectá-las aos plug-ins. Um controlador usado para [controle MIDI](midi-control.md) precisa estar ativado aqui.

## Quando os dispositivos mudam

O Curve cuida das mudanças de dispositivo mais comuns para você:

- Ele permanece na interface que você selecionou. Se essa interface for desconectada, o Curve fica em silêncio em vez de passar para outra saída, como os alto-falantes do Mac.
- Ele se reconecta e restaura o áudio quando a interface volta.
- Ele reinicia o áudio depois que o Mac sai do repouso.

## AirPods e troca automática

O macOS pode não trocar os AirPods automaticamente enquanto o Curve mantiver uma interface de áudio aberta. Para permitir isso, ative Settings > Release audio interface when idle.

O Curve então libera a interface após 5 segundos de silêncio e a reabre quando outro app começa a usá-la. Isso só acontece se o Curve puder ser reativado de forma confiável, portanto a interface não é liberada nestes casos:

- Sua entrada é um microfone físico ou uma entrada de linha em um dispositivo separado. Dispositivos virtuais como o BlackHole não são problema. O mesmo vale para uma entrada da sua interface de saída se o nó Audio Input estiver conectado no seu preset.
- Outro app está usando o dispositivo.
- O Curve está ocupado, por exemplo, gravando.
