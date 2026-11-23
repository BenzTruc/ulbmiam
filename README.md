# ULBMiam – Connected Kitchen

A systems-programming project written in C++ for the *Operating Systems*
course (INFO-F-201) at the Université Libre de Bruxelles. It simulates a
cafeteria kitchen that receives meal orders from several clients at the same
time, using processes, pipes, sockets, threads and signals.

## How it works

```
  recipes --(stdin)--> Cuisine <--(pipe)--> Stock
                       (server)
                          ^
                          | TCP socket (port 3333)
                          v
                 Client 1 ... Client n
```

- **`cuisine`** – the kitchen. It loads recipes (one CSV path per line on
  stdin), forks the stock process, and runs a TCP server on `127.0.0.1:3333`.
  Each client is handled in its own thread.
- **Stock process** – a child of the kitchen that keeps ingredient quantities
  and answers requests over anonymous pipes (query, take, add, display).
  Each new recipe restocks its ingredients, multiplied by a constant.
- **`commande`** – the client program used to query the menu and order dishes.
- **`afficheur-recettes.sh`** – a Bash script that prints the absolute path of
  every recipe CSV in a folder and checks for new ones every 30 seconds.

## ULBMiam protocol

A deliberately minimal text protocol: one request, one response.

| Request             | Response                           |
|---------------------|------------------------------------|
| `GET menu`          | `OK "<menu>"`                      |
| `GET plat <id> [n]` | `OK <id> <n>` – dish prepared      |
|                     | `EPUISE <id>` – not enough stock   |
|                     | `KO` – invalid request or id       |

## Concepts practised

- Process creation and communication with `fork` and anonymous pipes
- TCP sockets, one thread per client
- Thread synchronisation: a mutex protects the recipe list and the pipe so
  concurrent orders can never oversell the stock
- Environment variables (`IP_CUISINE`) and standard streams
- Signal handling (`SIGPIPE`, `SIGINT`, `SIGUSR1`)

## Build and run

```bash
g++ -std=c++17 -Wall -Wextra cuisine.cpp -o cuisine -pthread
./afficheur-recettes.sh recettes/ | ./cuisine
```

In another terminal:

```bash
./commande menu
./commande plat 1 2
```

## Status

- [x] Kitchen server, stock process and pipe protocol
- [x] ULBMiam requests and responses, one thread per client
- [x] Mutex-protected orders (150 simultaneous clients, no overselling)
- [x] `commande` client
- [x] `afficheur-recettes.sh`
- [ ] Signal handling and clean shutdown
