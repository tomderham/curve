# Ajustes de áudio e dispositivos

Selecione Settings > Audio Settings para escolher seus dispositivos de áudio e MIDI.

## Escolher interfaces e canais

Selecione uma interface de saída e, se o seu preset usar uma, uma interface de entrada. Depois ative os canais desejados. Eles aparecem como pinos cinza nos nós Audio Input e Audio Output.

Se você usa o [nó Interface Loopback (In)](loopback.md) do Curve, não precisa de uma interface de entrada.

O Curve lembra a taxa de amostragem, o tamanho do buffer e os canais de saída ativados de cada interface de saída, e os restaura quando você volta para essa interface.

## Seguir a saída do macOS

Ative **Use output interface selected in macOS** para deixar o macOS escolher a saída do Curve. Quando você escolhe uma saída nos Ajustes do Sistema, na Central de Controle ou na barra de menus, o Curve move o processamento para ela em cerca de um segundo. Se essa interface for desconectada, o Curve passa para a saída que o macOS usar em seguida. O menu Output do Curve fica esmaecido enquanto essa opção está ativada.

As conexões do seu preset com saídas que a nova interface não tem são mantidas, mas ficam em silêncio até você voltar para uma interface que as tenha. Uma correção feita para uma interface, como o EQ de um fone específico, é aplicada a qualquer saída para a qual o macOS mudar.

Com a opção ativada, selecione **Default Presets for each output interface...** para escolher um preset para cada interface. Quando o macOS muda para uma interface, o Curve carrega o preset padrão dela. Interfaces definidas como None mantêm o preset carregado. Ative **Also load at launch** para que o Curve carregue, ao iniciar, o preset padrão da interface que o macOS estiver usando, em vez do último preset usado. Se um preset padrão não for encontrado, o Curve mantém o preset atual e avisa na barra de status do editor.

## Manter o macOS na interface do Curve

**Force macOS system output to this interface** vem ativado por padrão. Ele mantém a saída do sistema do macOS na interface de saída do Curve, para que o [nó Interface Loopback (In)](loopback.md) receba todo o áudio do sistema. Desative-o se quiser escolher a saída do sistema por conta própria. Ele fica esmaecido enquanto Use output interface selected in macOS está ativado, porque a saída do macOS passa a ser sempre a do Curve.

## Taxa de amostragem e tamanho do buffer

Selecione aqui a taxa de amostragem e o tamanho do buffer. Um buffer menor significa menos latência, mas exige mais do seu Mac e pode causar falhas no áudio.

**Always enforce sample rate** vem desativado por padrão. Quando desativado, o Curve se adapta se a interface de áudio ou outro app alterar a taxa de amostragem do hardware. Quando ativado, o Curve força o retorno à taxa que você escolheu.

## Dispositivos MIDI

Ative as entradas e a saída MIDI que deseja usar. Use um nó MIDI Input ou MIDI Output para conectá-las aos plug-ins. Um controlador usado para [controle MIDI](midi-control.md) precisa estar ativado aqui.

## Quando os dispositivos mudam

O Curve cuida das mudanças de dispositivo mais comuns para você:

- Ele permanece na interface que você selecionou, a menos que Use output interface selected in macOS esteja ativado. Se essa interface for desconectada, o Curve fica em silêncio em vez de passar para outra saída, como os alto-falantes do Mac.
- Ele se reconecta e restaura o áudio quando a interface volta.
- Ele reinicia o áudio depois que o Mac sai do repouso.

## AirPods e troca automática

O macOS pode não trocar os AirPods automaticamente enquanto o Curve mantiver uma interface de áudio aberta. Para permitir isso, ative Settings > Release audio interface when idle.

O Curve então libera a interface após 5 segundos de silêncio e a reabre quando outro app começa a usá-la. Isso só acontece se o Curve puder ser reativado de forma confiável, portanto a interface não é liberada nestes casos:

- Sua entrada é um microfone físico ou uma entrada de linha em um dispositivo separado. Dispositivos virtuais como o BlackHole não são problema. O mesmo vale para uma entrada da sua interface de saída se o nó Audio Input estiver conectado no seu preset.
- Outro app está usando o dispositivo.
- O Curve está ocupado, por exemplo, gravando.
