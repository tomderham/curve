# EQ para fones de ouvido e AutoEQ

O Curve pode configurar um Parametric EQ a partir de um perfil de fone de ouvido medido, para que você corrija a resposta de frequência dos seus fones em todo o áudio do Mac.

## Usar um perfil de fone de ouvido do AutoEQ

Isso requer conexão com a internet.

1. Clique com o botão direito no fundo do editor e selecione Curve Plugins > Parametric EQ (import from AutoEQ...).
2. Na caixa de busca, digite o modelo do seu fone de ouvido, como HD 650 ou AirPods.
3. Selecione um resultado para ver a curva de resposta e os filtros.
4. Clique em Use this EQ. O Curve cria um nó Parametric EQ com esses ajustes.
5. [Conecte o nó](editor.md) à sua cadeia.
6. Selecione Save As Preset... para mantê-lo.

Para atualizar um nó Parametric EQ existente, clique nele com o botão direito e selecione Import EQ from AutoEQ...

Se um modelo de fone de ouvido estiver faltando, clique em Sync Index para verificar as novidades no banco de dados do AutoEQ.

## Ajustar com precisão ou encontrar outros perfis

- Para ajustar curvas-alvo e assinatura sonora, use o [autoeq.app](https://autoeq.app) e exporte a curva de EQ.
- Se seus fones de ouvido não estiverem na lista, experimente também o [squig.link](https://squig.link), que reúne medições de fones de ouvido e IEMs feitas pela comunidade.

## Importar um arquivo de EQ

O Curve importa curvas de EQ do AutoEQ, Squig.link e REW, em arquivos `.txt`, `.csv`, `.tsv` ou `.eq`.

- Para criar um novo nó, clique com o botão direito no fundo do editor e selecione Curve Plugins > Parametric EQ (import from file...), ou arraste o arquivo para a janela do editor.
- Para atualizar um nó Parametric EQ existente, clique nele com o botão direito e selecione Import EQ from file...

## Ajustar o EQ manualmente

Clique duas vezes no nó Parametric EQ para abrir a janela dele.
