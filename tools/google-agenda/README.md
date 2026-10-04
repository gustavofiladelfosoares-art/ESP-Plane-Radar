# Google Agenda no ESP Plane Radar

A página **Agenda** mostra os compromissos de hoje da sua Google Agenda e as
tarefas do Google Tasks. O aparelho não entra na sua conta Google. Você cria um
pequeno script na **sua** conta, que entrega só os itens de hoje por um link
secreto, e cola esse link na configuração do aparelho.

Leva uns 5 minutos e só precisa ser feito uma vez.

## 1. Criar o script

1. No computador, abra <https://script.google.com> e entre na sua conta Google.
2. Clique em **Novo projeto**.
3. Apague o que estiver escrito e cole todo o conteúdo de [`Code.gs`](Code.gs).
4. Clique no nome "Projeto sem título" e chame de **ESP Plane Radar**.
5. Para mostrar também as **tarefas**: no menu da esquerda, ao lado de
   **Serviços**, clique em **+**, escolha **Google Tasks API** e clique em
   **Adicionar**. Sem isso, aparecem só os eventos.
6. Clique em **Salvar** (ícone de disquete).

## 2. Publicar como app da Web

1. Clique em **Implantar → Nova implantação**.
2. Na engrenagem de **Selecionar tipo**, escolha **App da Web**.
3. Em **Executar como**, deixe **Eu**. Em **Quem pode acessar**, escolha
   **Qualquer pessoa**. Assim o aparelho consegue ler sem fazer login.
4. Clique em **Implantar** e depois em **Autorizar acesso**. Escolha sua conta.
   Se aparecer "O Google não verificou este app", clique em **Avançado → Acessar
   ESP Plane Radar (não seguro)**. O app é seu, criado por você agora mesmo.
   Depois clique em **Permitir**.
5. Copie o **URL do app da Web**. Ele termina em `/exec`.

Para conferir, abra esse link no navegador. Deve aparecer algo como
`{"events":[...],"tasks":[...]}`.

## 3. Colar no aparelho

1. Com o celular na mesma rede Wi‑Fi do aparelho, abra
   **http://plane-radar.local**, ou o IP que aparece no monitor serial.
2. Toque em **Setup** e cole o link no campo **Link da Google Agenda**.
3. Salve. Em alguns segundos a página **Agenda** aparece no ciclo de páginas.
   Ela é atualizada a cada 10 minutos, e **dois toques** no botão forçam a
   atualização na hora.

## Privacidade

- Quem tiver o link consegue ver os **títulos dos seus compromissos e tarefas de
  hoje**. Trate o link como uma senha e não o publique.
- O link fica guardado **só na memória do aparelho**. Ele não vai para o
  firmware nem para o GitHub. Segurar o BOOT por 3 s apaga o link junto com o
  resto da configuração.
- Para cortar o acesso: em script.google.com, **Implantar → Gerenciar
  implantações → Arquivar**.
