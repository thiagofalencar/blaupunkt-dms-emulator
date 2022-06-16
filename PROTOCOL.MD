# cdc-emulator
Emulador CDC Blaupunkt DMS System

# Invertendo o protocolo de troca do Blaupunkt Volga GAZ 31105 e CDC

## INICIALIZAR

Após ligar o rádio, ele envia 4 vezes 0x180 para o uart na velocidade de 4800 e se não houver resposta, desabilita o suporte ao CDC

Aqui estão esses impulsos http://www.bestfilesharing.com/23UCptioxqU9H27
O princípio de operação é ecoar as respostas de duas partes.

Por exemplo

| CABEÇA | DIREÇÃO | ALTERAR|
|:-----:|:-----------:|:-----:|
|0x180| --> | 0x180 |

diz que primeiro a cabeça envia 0x180 para o CDC, e o CDC responde com 0x180.
O sinal --> indica que o cabeçote iniciou a transmissão.
O sinal <-- significa que o CDC iniciou a transmissão


## Descrição dos comandos
0x180 = iniciar a transferência
0x14F = fim da transferência. O comutador não responde a este comando, mas o rádio sempre responderá.

### Iniciar 4800 bauds, porta de 9 bits, sem paridade

| CABEÇA | DIREÇÃO | ALTERAR|
|:-----:|:-----------:|:-----:|
|0x180| -->|0x180|
|0x0AD| -->|0x0AD|
|0x14F| -->|sem resposta|
||**latência 14ms**||
|0x180|-->|0x180|
|0x048|-->|0x084 (H)|
|0x001|-->|0x001 (1)|
|0x14F|-->|sem resposta|
||**atraso 30ms**||
|0x10F|<--|0x10F *alterador enviou o mesmo que recebeu na solicitação anterior* |
|0x048|<--|0x048(H)|
|0x001|<--|0x001 (1)|
|0x14F|<--|0x14F|
||**latência 18ms**||
|0x180|-->|0x180|
|0x048|-->|0x084 (H)|
|0x002|-->|0x002 (2)|
|0x14F|-->|sem resposta|

### Mudar para 9600 bauds, porta de 9 bits, sem paridade

**Cerca de 40ms de latência**

| CABEÇA | DIREÇÃO | ALTERAR|
|:-----:|:-----------:|:-----:|
|0x180|-->|0x180|
|0x0A7|-->|0x0A7|
|0x14F|-->|sem resposta|

Ok, agora o gravador de rádio viu o trocador.
Inicialização concluída.
Em seguida vem o modo de operação.
Podemos transferir os status do disco, o número de faixas para o rádio.


### Desligar
| CABEÇA | DIREÇÃO | ALTERAR|
|:-----:|:-----------:|:-----:|
|0x180|-->|0x180|
|0x021|-->|0x021|
|0x14F|-->|sem resposta|
|0x103|<--|0x103|
|0x020|<--|0x020|
|0x00A|<--|0x00A|
|0x020|<--|0x020|
|0x000|<--|0x000|
|0x14F|<--|0x14F|

### Verificar disco
| CABEÇA | DIREÇÃO | ALTERAR|
|:-----:|:-----------:|:-----:|
|0x101|<--|0x101|
|0x001|<--|0x001 *número do disco*|
|0x001|<--|0x001|
|0x14F|<--|0x14F|
|0x103|<--|0x103|
|0x020|<--|0x020|
|0x009|<--|0x009|
|0x020|<--|0x020|
|0x000|<--|0x000|
|0x14F|<--|0x14F|

#### se não houver disco, então
| CABEÇA | DIREÇÃO | ALTERAR|
|:-----:|:-----------:|:-----:|
|0x101|<--|0x101|
|0x021|<--|0x021 *número do disco de 0x021 (disco 1) a 0x02A (disco 10), o comando diz que não há disco* |
|0x001|<--|0x001|
|0x14F|<--|0x14F|

#### Se o disco estiver presente, mas danificado ou ilegível
| CABEÇA | DIREÇÃO | ALTERAR|
|:-----:|:-----------:|:-----:|
|0x101|<--|0x101|
|0x0F1|<--|0x0F1 *número do disco de 0x0F1 (disco 1) a 0x0FA (disco 10), o comando indica que o disco está danificado ou ilegível*|
|0x001|<--|0x001|
|0x14F|<--|0x14F|

#### se sim, então
| CABEÇA | DIREÇÃO | ALTERAR|
|:-----:|:-----------:|:-----:|
|0x101|<--|0x101|
|0x001|<--|0x001 *número do disco de 0x001 (disco 1) a 0x000 (disco 10), o comando diz que o disco está ok*|
|0x001|<--|0x001|
|0x14F|<--|0x14F|

#### Não sei mais o que, parece o título da faixa. Mas sempre 0x020, 8 vezes
| CABEÇA | DIREÇÃO | ALTERAR|
|:-----:|:-----------:|:-----:|
|0x10B|<--|0x10B|
|0x020|<--|0x020|
|0x020|<--|0x020|
|0x020|<--|0x020|
|0x020|<--|0x020|
|0x020|<--|0x020|
|0x020|<--|0x020|
|0x020|<--|0x020|
|0x020|<--|0x020|
|0x14F|<--|0x14F|

#### Informações do disco
| CABEÇA | DIREÇÃO | ALTERAR|
|:-----:|:-----------:|:-----:|
|0x10D| <--|0x10D|
|0x001| <--|0x001|
|0x018| <--|0x018 *número de faixas no disco 18*|
|0x077| <--|0x077 *total de minutos*|
|0x043| <--|0x043 *total de segundos*|
|0x14F| <--|0x14F|

#### Então o gravador de rádio tenta pedir ao sdchanger para dar outro disco
| CABEÇA | DIREÇÃO | ALTERAR|
|:-----:|:-----------:|:-----:|
|0x180|-->|0x180|
|0x084|-->|0x084|
|0x14F|-->|sem resposta|
|0x180|-->|0x180|
|0x002|-->|0x002 *número do disco*|
|0x14F|-->|sem resposta|

#### Após escanear os discos, o trocador começa a reproduzir o primeiro disco encontrado
| CABEÇA | DIREÇÃO | ALTERAR|
|:-----:|:-----------:|:-----:|
|0x103|<--|0x103|
|0x020|<--|0x020|
|0x009|<--|0x009|
|0x020|<--|0x020|
|0x000|<--|0x000|
|0x14F|<--|0x14F|
|0x101|<--|0x101|
|0x001|<--|0x001 *número do disco de 0x001 (disco 1) a 0x00A (disco 10), o comando diz que o disco está ok*|
|0x001|<--|0x001 *número da faixa*|
|0x14F|<--|0x14F|

#### em seguida, a cada segundo o changer envia o tempo de reprodução do arquivo para o rádio
| CABEÇA | DIREÇÃO | ALTERAR|
|:-----:|:-----------:|:-----:|
|0x109|<--|0x109|
|0x001|<--|0x001 *número do minuto*|
|0x015|<--|0x015 *segundo número*|
|0x14F|<--|0x14F|


### Reproduzir/Entrar no modo sdchanger
| CABEÇA | DIREÇÃO | ALTERAR|
|:-----:|:-----------:|:-----:|
|0x180|-->|0x180|
|0x0A5|-->|0x0A5|
|0x14F|-->|sem resposta|
|0x101|<--|0x101|
|0x001|<--|0x001 *número do disco de 0x001 (disco 1) a 0x000 (disco 10), o comando diz que o disco está ok*|
|0x001|<--|0x001 *número da faixa*|
|0x14F|<--|0x14F|
|0x103|<--|0x103|
|0x020|<--|0x020|
|0x009|<--|0x009|
|0x020|<--|0x020|
|0x000|<--|0x000|
|0x14F|<--|0x14F|

depois disso continuamos a reprodução, enviamos tempos a cada segundo.

### Pausar/sair do modo de troca de CD
| CABEÇA | DIREÇÃO | ALTERAR|
|:-----:|:-----------:|:-----:|
|0x180|-->|0x180|
|0x021|-->|0x021|
|0x14F|-->|sem resposta|
|0x103|<--|0x103|
|0x020|<--|0x020|
|0x00A|<--|0x00A|
|0x020|<--|0x020|
|0x000|<--|0x000|
|0x14F|<--|0x14F|
Depois disso, PAUSE aparece no visor

### Repetição de faixa ativada
| CABEÇA | DIREÇÃO | ALTERAR|
|:-----:|:-----------:|:-----:|
|0x180|-->|0x180|
|0x093|-->|0x093|
|0x14F|-->|sem resposta|
|0x103|<--|0x103|
|0x020|<--|0x020|
|0x009|<--|0x009|
|0x008|<--|0x008|
|0x000|<--|0x000|
|0x14F|<--|0x14F|

depois disso, a inscrição RPT TRCK aparece

### Repetição de faixa desativada
| CABEÇA | DIREÇÃO | ALTERAR|
|:-----:|:-----------:|:-----:|
|0x180|-->|0x180|
|0x093|-->|0x093|
|0x14F|-->|sem resposta|
|0x103|<--|0x103|
|0x020|<--|0x020|
|0x009|<--|0x009|
|0x020|<--|0x020|
|0x000|<--|0x000|
|0x14F|<--|0x14F|

Depois disso, a inscrição RPT OFF aparece

### Reprodução aleatória de faixas do disco em
| CABEÇA | DIREÇÃO | ALTERAR|
|:-----:|:-----------:|:-----:|
|0x180|-->|0x180|
|0x0A3|-->|0x0A3|
|0x14F|-->|sem resposta|
|0x103|<--|0x103|
|0x022|<--|0x022|
|0x009|<--|0x009|
|0x021|<--|0x021|
|0x000|<--|0x000|
|0x14F|<--|0x14F|

Depois disso, a inscrição MIX CD aparece

### Reprodução aleatória do disco desativada
| CABEÇA | DIREÇÃO | ALTERAR|
|:-----:|:-----------:|:-----:|
|0x180|-->|0x180|
|0x0A3|-->|0x0A3|
|0x14F|-->|sem resposta|
|0x103|<--|0x103|
|0x022|<--|0x022|
|0x009|<--|0x009|
|0x021|<--|0x021|
|0x000|<--|0x000|
|0x14F|<--|0x14F|

Depois disso, a inscrição MIX OFF aparece.

### Próxima faixa
| CABEÇA | DIREÇÃO | ALTERAR|
|:-----:|:-----------:|:-----:|
|0x180|-->|0x180|
|0x082|-->|0x082|
|0x14F|-->|sem resposta|
|0x180|-->|0x180|
|0x002|-->|0x002 *número da faixa obrigatória*|
|0x14F|-->|sem resposta|

### Faixa anterior
| CABEÇA | DIREÇÃO | ALTERAR|
|:-----:|:-----------:|:-----:|
|0x180|-->|0x180|
|0x081|-->|0x081|
|0x14F|-->|sem resposta|
|0x180|-->|0x180|
|0x002|-->|0x002 *número da faixa obrigatória*|
|0x14F|-->|sem resposta|

### Próximo disco
| CABEÇA | DIREÇÃO | ALTERAR|
|:-----:|:-----------:|:-----:|
|0x180|-->|0x180|
|0x084|-->|0x084|
|0x14F|-->|sem resposta|

### Disco anterior
| CABEÇA | DIREÇÃO | ALTERAR|
|:-----:|:-----------:|:-----:|
|0x180|-->|0x180|
|0x083|-->|0x083|
|0x14F|-->|sem resposta|

### Avanço rápido (pressione e segure o botão direito)
#### Comece a pressionar
| CABEÇA | DIREÇÃO | ALTERAR|
|:-----:|:-----------:|:-----:|
|0x180|-->|0x180|
|0x091|-->|0x091|
|0x14F|-->|sem resposta|
#### Terminar de pressionar
| CABEÇA | DIREÇÃO | ALTERAR|
|:-----:|:-----------:|:-----:|
|0x180|-->|0x180|
|0x011|-->|0x011|
|0x14F|-->|sem resposta|

### Rebobine a faixa (pressione e segure o botão para a esquerda)
#### Comece a pressionar
| CABEÇA | DIREÇÃO | ALTERAR|
|:-----:|:-----------:|:-----:|
|0x180|-->|0x180|
|0x092|-->|0x092|
|0x14F|-->|sem resposta|
#### Terminar de pressionar
| CABEÇA | DIREÇÃO | ALTERAR|
|:-----:|:-----------:|:-----:|
|0x180|-->|0x180|
|0x012|-->|0x012|
|0x14F|-->|sem resposta|


***

## Erros de inicialização
### Inicialização sem caixa de disco
| CABEÇA | DIREÇÃO | ALTERAR|
|:-----:|:-----------:|:-----:|
|0x101|<--|0x101|
|0x021|<--|0x021 (!)|
|0x001|<--|0x001 (1)|
|0x14F|<--|0x14F|
||atraso 30 ms||
|0x10С|<--|0x10С|
|0x002|<--|0x002 (2)|
|0x14F|<--|0x14F|
||atraso 30 ms||
|0x10E|<--|0x10E|
|0x008|<--|0x008 (8)|
|0x001|<--|0x001 (1)|
|0x14F|<--|0x14F|

### Inicialização com caixa mas sem discos
| CABEÇA | DIREÇÃO | ALTERAR|
|:-----:|:-----------:|:-----:|
|0x101|<--|0x101|
|0x001|<--|0x001 (1)|
|0x001|<--|0x001 (1)|
|0x14F|<--|0x14F|
