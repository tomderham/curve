# Controle MIDI

Um controlador ou teclado MIDI pode tocar plug-ins de instrumento, alterar parâmetros de plug-ins, ignorar nós e alternar presets. Ative-o nas entradas MIDI em [Audio Settings](audio-settings.md).

Para tocar um instrumento, ou enviar MIDI a qualquer outro plug-in, é preciso um nó MIDI Input conectado a esse plug-in. Controlar parâmetros, bypass e presets com o MIDI Learn não exige um nó MIDI Input nem conexões no preset.

## Tocar um plug-in de instrumento

1. Clique com o botão direito no fundo do editor e adicione seu plug-in de instrumento.
2. Clique com o botão direito no fundo do editor e selecione MIDI I/O > MIDI Input.
3. Arraste do pino vermelho do nó MIDI Input até o pino vermelho na parte superior do instrumento. Pinos e conexões MIDI são vermelhos, e pinos e conexões de áudio são verdes.
4. Arraste dos pinos de saída verdes do instrumento até o nó Audio Output.
5. Selecione Save As Preset... para manter a configuração.

Plug-ins de efeito que respondem a MIDI são conectados da mesma forma. Para enviar MIDI de um plug-in a um dispositivo externo, ative o dispositivo como saída MIDI em Audio Settings e conecte o pino de saída vermelho do plug-in a um nó MIDI Output.

## Mapear um controle com o MIDI Learn

1. Clique com o botão direito em qualquer nó e selecione MIDI Learn. O editor ganha uma borda destacada e a barra de status orienta você.
2. Escolha o que controlar:
   - Para um parâmetro de plug-in, abra a janela do plug-in e mova o controle.
   - Para o bypass de um nó, clique no botão liga/desliga dele.
3. Mova um knob, fader ou botão do seu controlador. O mapeamento é feito na hora, e você pode continuar mapeando outros controles.
4. Pressione Esc, ou selecione MIDI Learn novamente, para concluir.

Os mapeamentos são salvos com o preset, portanto use Save As Preset... para mantê-los.

## Remover um mapeamento

- Para remover o mapeamento do parâmetro ou bypass selecionado, pressione Delete durante o MIDI Learn.
- Para remover todos os mapeamentos de um nó, clique nele com o botão direito e selecione Clear MIDI Mappings.

## Como os controles mapeados se comportam

- Knobs e faders (CC) cobrem toda a faixa de um parâmetro.
- Notas e mudanças de programa alternam o destino.
- Para o bypass, o Curve identifica se um botão CC é momentâneo, em que cada toque alterna o bypass, ou com trava, em que o bypass acompanha o estado do botão.

Os nós com mapeamentos mostram um pequeno ícone de conector MIDI. Passe o ponteiro sobre o nó para ver os mapeamentos dele na barra de status.

Mensagens MIDI mapeadas não são repassadas aos plug-ins pelo nó MIDI Input. Todas as demais mensagens MIDI são.

## Alternar presets a partir de um controlador

Abra Settings > MIDI Control. Estes ajustes valem para todos os presets:

- **Program Change Selects Preset:** desativado por padrão. A mudança de programa 0 carrega o primeiro preset do menu, 1 o segundo, e assim por diante, na mesma ordem dos [atalhos de teclas numéricas](keyboard-shortcuts.md).
- **Next Preset Control** e **Previous Preset Control:** selecione um deles e pressione o botão do controlador que deseja usar.
- **Clear Preset Controls:** remove essas duas atribuições de botões.

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
