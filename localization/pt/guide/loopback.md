# Loopback e áudio do sistema

O loopback permite enviar o áudio do macOS pelo Curve antes que ele chegue aos seus fones de ouvido ou alto-falantes. É assim que você aplica EQ a tudo o que o Mac reproduz.

## Usar o nó Interface Loopback do Curve

Não é preciso nenhum software ou driver adicional.

1. Em [Audio Settings](audio-settings.md), selecione a interface de saída pela qual deseja ouvir o áudio.
2. No seu preset, use o nó Interface Loopback (In) como entrada em vez de Audio Input.
3. Conecte-o, passando pelos seus plug-ins, ao nó Audio Output.

O nó recebe, com exatidão de bits, o áudio que de outra forma teria ido direto para a interface de saída. Se ele não estiver lá, adicione-o em Audio I/O no menu de contexto do editor.

## Enviar todo o áudio do sistema para o Curve

**Force macOS System Audio to loopback** no menu Settings vem ativado por padrão. Ele mantém a saída do sistema do macOS na interface selecionada no Curve. Desative-o se quiser escolher a saída do sistema por conta própria.

## Usar o loopback da sua interface de áudio

Como alternativa, se sua interface tiver loopback por hardware, essa pode ser uma boa opção, pois pode minimizar a latência. Por exemplo, com o RME TotalMix:

1. Use a função Loopback do TotalMix para enviar um par de canais de saída a um par livre de canais de entrada.
2. No Curve, selecione esses canais de entrada e use o nó Audio Input.
3. Selecione alguns dos canais de reprodução por software como saídas do Curve.
4. No TotalMix, atribua esses canais de reprodução às suas saídas de hardware.

Não use o nó Interface Loopback (In) com esta configuração.

## Importante: pinos de loopback sem uso

Se o nó Interface Loopback (In) estiver conectado, o áudio direto desses canais deixa de chegar ao dispositivo de saída. Se ele estiver em um preset em que você não o usa, remova-o ou deixe os pinos dele desconectados, ou o áudio poderá ser silenciado inesperadamente.

## Como funciona o redirecionamento

Os canais do Stream 0, o fluxo principal da interface de saída selecionada, são redirecionados para o nó quando os pinos dele estão conectados. Os canais que o driver da interface atribui a outro fluxo não são afetados.
