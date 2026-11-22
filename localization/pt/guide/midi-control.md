# Controle MIDI

Um controlador ou teclado MIDI pode tocar plug-ins de instrumento, alterar parâmetros de plug-ins, colocar nós em bypass ou silenciá-los e alternar presets. Ative-o nas entradas MIDI em [Audio Settings](audio-settings.md).

## Escolher como controlar um plug-in

Há duas formas de controlar um plug-in pelo seu controlador:

- **Conectar MIDI ao plug-in.** Um nó MIDI Input envia MIDI diretamente ao pino de entrada âmbar do plug-in, e o plug-in decide o que fazer com ele. Use esta forma para tocar um instrumento e para aproveitar qualquer mapeamento MIDI integrado ao plug-in.
- **Usar o MIDI Learn do Curve.** O próprio Curve lê o controlador e altera para você os parâmetros, o bypass ou o mudo do plug-in. Isso não exige um nó MIDI Input nem conexões. Use esta forma para plug-ins sem pino de entrada âmbar e para o bypass e o mudo.

O MIDI Learn só alcança os parâmetros que um plug-in disponibiliza ao host. Alguns plug-ins, principalmente instrumentos grandes, disponibilizam apenas parte dos controles. Se mover um controle na janela do plug-in durante o MIDI Learn não tiver efeito, o Curve não poderá mapear esse controle.

Se um plug-in tiver um pino de entrada âmbar e um recurso próprio de MIDI Learn, essa costuma ser a opção mais fácil, porque normalmente alcança todos os controles. Conecte um nó MIDI Input ao plug-in e configure o mapeamento na janela do plug-in. O manual dele explica como.

Use um único método para cada botão. Um controle mapeado com o MIDI Learn do Curve não chega aos plug-ins pelo nó MIDI Input, portanto o plug-in não responderá a ele.

## Tocar um plug-in de instrumento

1. Clique com o botão direito no fundo do editor e adicione seu plug-in de instrumento.
2. Clique com o botão direito no fundo do editor e selecione MIDI I/O > MIDI Input.
3. Arraste do pino âmbar do nó MIDI Input até o pino âmbar na parte superior do instrumento. Pinos e conexões MIDI são âmbar, e pinos e conexões de áudio são cinza.
4. Arraste dos pinos de saída cinza do instrumento até o nó Audio Output.
5. Selecione Save As Preset... para manter a configuração.

Plug-ins de efeito que respondem a MIDI são conectados da mesma forma. Para enviar MIDI de um plug-in a um dispositivo externo, ative o dispositivo como saída MIDI em Audio Settings e conecte o pino de saída âmbar do plug-in a um nó MIDI Output.

## Mapear um controle com o MIDI Learn

1. Clique no ícone de menu no canto superior esquerdo do editor e selecione MIDI Learn. O editor ganha uma borda destacada e a barra de status orienta você.
2. Escolha o que controlar:
   - Para um parâmetro de plug-in, abra a janela do plug-in e mova o controle.
   - Para o bypass de um nó, clique no botão liga/desliga dele.
   - Para silenciar um nó, clique nele com o botão direito e selecione Mute.
   - Para o andamento, Tap ou Start/Stop, clique nele no canto superior direito do editor.
3. Mova um knob, fader ou botão do seu controlador. O mapeamento é feito na hora, e você pode continuar mapeando outros controles.
4. Pressione Esc, ou selecione MIDI Learn novamente, para concluir.

Os mapeamentos são salvos com o preset, portanto use Save As Preset... para mantê-los.

## Remover um mapeamento

- Para remover o mapeamento do parâmetro, bypass ou mudo selecionado, pressione Delete durante o MIDI Learn.
- Para remover todos os mapeamentos de um nó, clique nele com o botão direito e selecione Clear MIDI Mappings.
- Para passar um controle para outro alvo, remova o mapeamento antigo, pois mapeá-lo de novo adiciona um alvo.

## Como os controles mapeados se comportam

- Knobs e faders (CC) cobrem toda a faixa de um parâmetro. Você pode restringi-la em MIDI Mappings for Current Preset... (veja abaixo).
- Notas e mudanças de programa alternam o destino.
- Para o bypass e o mudo, e para parâmetros liga/desliga como Record, o Curve identifica se um botão CC é momentâneo, em que cada toque alterna o estado, ou com trava, em que o estado acompanha o do botão.
- Um controle pode controlar vários alvos: mapeie-o de novo com o MIDI Learn para adicionar outro. Um controle com vários alvos não altera os parâmetros de nós mudos, então nas [cenas](presets.md) um knob pode controlar o instrumento que cada cena usa.
- Um knob ou fader no andamento percorre de 40 a 240 BPM, faixa que você pode restringir em MIDI Mappings for Current Preset.... Um botão em Tap marca o andamento, e um em Start/Stop inicia e para o transporte.

Os nós com mapeamentos mostram um pequeno ícone de conector MIDI. Passe o ponteiro sobre o nó para ver os mapeamentos dele na barra de status.

Mensagens MIDI mapeadas não são repassadas aos plug-ins pelo nó MIDI Input. Todas as demais mensagens MIDI são.

## Ajustar um mapeamento

Clique no ícone de menu no canto superior esquerdo da janela do editor e selecione MIDI Mappings for Current Preset... para ver os mapeamentos do preset carregado. A janela pode ficar aberta enquanto você usa o MIDI Learn ou troca de preset. Para cada mapeamento, você pode definir:

- **Mode:** Range, em que um botão ou fader define o valor; Toggle, em que cada toque alterna entre dois valores; ou Follow, em que o alvo segue um botão e fica ativado enquanto ele está pressionado ou travado.
- **From e To:** os valores entre os quais um botão ou fader se move. Uma faixa mais estreita dá passos mais finos, por exemplo -20 dB a +6 dB em um nó Gain. Para Toggle e Follow, são os valores de desligado e ligado. Digite um valor na unidade do parâmetro ou uma porcentagem como 25%.
- **Invert:** inverte o controle.

Mapeamentos de bypass e de mudo não têm From nem To. Clique em × para remover um mapeamento. Os mapeamentos são salvos com o preset, então use Save As Preset... para manter suas alterações.

## Alternar presets a partir de um controlador

Selecione Settings > Preferences... e abra a aba MIDI. Estes ajustes valem para todos os presets:

- **Program Change Selects Preset:** desativado por padrão. A mudança de programa 0 carrega o primeiro preset do menu, 1 o segundo, e assim por diante, na mesma ordem dos [atalhos de teclas numéricas](keyboard-shortcuts.md).
- **Next Preset Control** e **Previous Preset Control:** clique em MIDI Learn... ao lado de um deles e pressione o botão do controlador que deseja usar.
- **Clear Preset Controls:** remove essas duas atribuições de botões.

Se o preset carregado mapear o mesmo controle, o mapeamento do preset é usado. Os presets não mudam por MIDI enquanto uma janela como Save As Preset... ou Audio Settings estiver aberta.

## Controles dos plug-ins do Curve

| Plug-in | Controles |
|---|---|
| Gain | Ganho e mudo |
| Pan | Pan |
| Clipper | Teto |
| Stereo Utility | Modo e largura |
| Headphone Crossfeed | Intensidade |
| Audio Recorder | Iniciar e parar a gravação |

Headphone Speaker Emulation, Invert Phase, Mid/Side Encoder e Mid/Side Decoder não têm ajustes, então mapeie o bypass deles. Delay não pode ser mapeado.
