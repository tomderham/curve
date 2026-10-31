# Curve Plugins

O Curve inclui estes plug-ins. Para adicionar um plug-in a um preset, clique com o botão direito no fundo do editor e selecione Curve Plugins.

## EQ

- **Parametric EQ:** um atalho para o AUNBandEQ, o EQ paramétrico multibanda da Apple, muito usado para calibração. Multicanal. O mesmo menu permite [importar um perfil de fone de ouvido ou um arquivo de EQ](eq-and-autoeq.md).

## Processamento para fones de ouvido

- **Headphone Crossfeed:** crossfeed para uma imagem estéreo mais natural em fones de ouvido, com intensidade ajustável. Estéreo.
- **Headphone Speaker Emulation:** emula monitores de estúdio de campo médio em fones de ouvido, incluindo crossfeed. Estéreo.

## Nível e segurança

- **Gain:** ajuste de volume de -60 dB a +60 dB, com botão de mudo. Multicanal.
- **Clipper:** corta cada canal em um teto de -50 a 0 dBFS, como proteção contra reforços de EQ, com indicador de clipping. Só adiciona proteção quando o teto está abaixo de 0 dBFS. Sem latência. Multicanal.
- **Meter:** mostra uma de quatro visualizações do que você conectar a ele. Levels mostra RMS, com média de cerca de 300 ms (uma senoide lê 3 dB abaixo do pico), true peak em dBTP e correlação estéreo, em que +1 é mono e -1 está fora de fase. VU mostra medidores de agulha com subida de 300 ms, e você pode escolher o nível que lê 0 VU: -20, -18, -14 ou -10 dBFS. Loudness mostra LUFS momentâneo, de curto prazo e integrado (ITU-R BS.1770 / EBU R128), com o histórico do último minuto. Spectrum mostra a potência combinada dos dois canais, e L/R os sobrepõe separadamente. Clip acende quando uma amostra ultrapassa 0 dBFS e fica aceso até ser clicado. Só mede enquanto sua janela está aberta, e a loudness integrada recomeça sempre que a janela é aberta. Por padrão, não tem saídas, então conecte-o como derivação a partir de qualquer pino. Para colocá-lo em linha, adicione saídas com Configure Audio I/O, e o áudio passará sem alterações. Sem latência. Mono ou estéreo.
- **Invert Phase:** inverte a polaridade de cada canal. Multicanal.

## Roteamento e estéreo

- **Pan:** posiciona uma entrada mono na saída estéreo, com lei de pan de 0, -3, -4,5 ou -6 dB no centro. Com uma entrada estéreo, definida em Configure Audio I/O, funciona como controle de balanço.
- **Stereo Utility:** troca esquerda e direita, soma mono, somente esquerda ou direita, e largura estéreo de 0 a 200%. Estéreo.
- **Mid/Side Encoder:** converte esquerda e direita em Mid no primeiro pino e Side no segundo. Estéreo.
- **Mid/Side Decoder:** converte Mid e Side de volta em esquerda e direita. Coloque um codificador e um decodificador em torno do processamento separado dos dois pinos. O percurso completo é exato em bits. Estéreo.
- **Delay:** atraso em amostras inteiras de até 2 segundos, mostrado em amostras, milissegundos ou centímetros, para alinhar alto-falantes ou sincronizar áudio com vídeo. Alterá-lo durante a reprodução pode causar um breve clique. Multicanal.

## Gravação

- **Audio Recorder:** grava arquivos WAV na sua Mesa. Multicanal.

## Controles atribuíveis por MIDI Learn

Consulte [Controle MIDI](midi-control.md) para ver o que cada plug-in disponibiliza.
