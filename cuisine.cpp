#include <iostream>
#include <sys/types.h>  // pid_t
#include <unistd.h>     // fork
#include <map>
#include <string>
#include <pthread.h>
#include <fstream>
#include <sstream>
#include <cstring>
#include <vector>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <filesystem>
#include <mutex>
#include <csignal>
#include <arpa/inet.h>
#include <atomic>
#include <sys/wait.h>

#define READ 0
#define WRITE 1

const int multiplicateur = 100;
const int port = 3333;
std::atomic<bool> arret(false);

//STRUCTURES
struct Item{
    char itemName[64];
    int quantity;
};


struct Reciept{
    std::string nom;
    std::vector <Item> ingredients;
};

//---

class Stock{
    std::map<std::string, int> items;
    const int read_fd, write_fd;

    void add(const std::string& name, int quantity){
            items[name] += quantity;
        }

        bool take(const std::string& name, int quantity){
            auto it = items.find(name);
            if (it == items.end() || it->second < quantity){
                return false;
            }
            it->second -= quantity;
            return true;
        }

        int quantity(const std::string& name){
            auto it = items.find(name);
            return it != items.end() ? it->second : 0;
        }

    public:
        Stock(const int &r_fd, const int &w_fd) : read_fd(r_fd), write_fd(w_fd){}

        void sendItemQuantity(){
            char name[64];
            if (read(read_fd, name, 64) != 64) return;
            name[64-1] = '\0';
            int quantity = this->quantity(std::string(name));
            write(write_fd, &quantity, sizeof(int));
        }

        void takeItem(){
            Item itemToRemove;
            if (read(read_fd, &itemToRemove, sizeof(itemToRemove)) != sizeof(itemToRemove)) return;

            itemToRemove.itemName[64-1] = '\0';
            if (take(std::string(itemToRemove.itemName), itemToRemove.quantity)){
                char response[4] = "OK\n";
                write(write_fd, &response, sizeof(response));
            }
            else{
                //Il y a trop peu d'éléments
                char response[4] = "KO\n";
                write(write_fd, &response, sizeof(response));
            }
        }

        void addItem(){
            Item itemToAdd;
            if (read(read_fd, &itemToAdd, sizeof(itemToAdd)) != sizeof(itemToAdd)) return;
            itemToAdd.itemName[63] = '\0';
            add(std::string(itemToAdd.itemName), itemToAdd.quantity);

        }

        void showStock(){
            for (const auto& [item, quantity] : items){
                std::cout << item << " : " << quantity << std::endl;
            }
        }

};

volatile sig_atomic_t stock_stop = 0;

void stockSignalHandler(int){
    stock_stop = 1;
}

void stock(int read_fd, int write_fd){
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = stockSignalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGPIPE, &sa , nullptr);


    int operation;
    Stock stock(read_fd, write_fd);
    while (!stock_stop)
    {
        ssize_t n = read(read_fd, &operation, sizeof(int));
        if (n <= 0) break;
        switch (operation){
            case 1:
            stock.sendItemQuantity();
            break;
            case 2:
            stock.takeItem();
            break;
            case 3:
            stock.addItem();
            break;
            case 4:
            stock.showStock();
            break;
            default:
            //erreur
            std::cout << "ERROR" << std::endl;

        }
    }
    close(read_fd);
    close(write_fd);
}



std::vector <Item> readCsv(const std::string& filename){
    std::vector<Item> items;
    std::ifstream file(filename);
    //std::string line;

    if (!file.is_open()){
        std::cout << "Impossible d'ouvir le fichier au chemin : " << filename << std::endl;
        return items;
    }
    std::string line;
    while (std::getline(file,line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string element, quantityStr;

        if(std::getline(ss, element, ';') && std::getline(ss, quantityStr)){
            try {
                Item item;
                std::memset(&item, 0, sizeof(item));
                std::strncpy(item.itemName, element.c_str(), 64-1);
                item.quantity = std::stoi(quantityStr);
                items.push_back(item);
            } catch (const std::exception&){
                std::cerr << "Ligne invalide : " << line << std::endl;
            }
        }
        
    }
    return items;
}



class Cuisine{
    std::vector <Reciept> ListeRecettes;
    const int read_fd, write_fd;
    std::mutex mutex;

    void sendOrderToStock(const char name[64], const int quantity){
            int commandNumber = 3;

            Item itemToAdd;
            memset(&itemToAdd, 0, sizeof(itemToAdd));
            std::strncpy(itemToAdd.itemName, name, 64-1);
            itemToAdd.quantity = quantity;
            write(write_fd, &commandNumber, sizeof(commandNumber));
            write(write_fd, &itemToAdd, sizeof(itemToAdd));
            
        }


    public:
        Cuisine(const int &w_fd, const int &r_fd) : read_fd(r_fd), write_fd(w_fd){}
        void addReciept(Reciept recette){
            std::lock_guard<std::mutex> guard(mutex);
            ListeRecettes.push_back(recette);
            for (const Item& item : recette.ingredients){
                sendOrderToStock(item.itemName, item.quantity * multiplicateur);
            }

        };

        std::string orderItem(int itemId, int quantity){
            std::lock_guard<std::mutex> guard(mutex);
            if ((itemId > (int)ListeRecettes.size()) or itemId < 1){
                return "KO";
            }


            int commandNumber = 1;
            std::vector <Item> listeIngredients = ListeRecettes[itemId-1].ingredients;
            for(const Item& ingredient : listeIngredients){

                write(write_fd, &commandNumber, sizeof(commandNumber));
                write(write_fd, &ingredient.itemName, sizeof(ingredient.itemName));
                int stockQuantity;
                read(read_fd, &stockQuantity, sizeof(stockQuantity));
                if ((ingredient.quantity * quantity) > stockQuantity){
                    return "EPUISE";
                }
            }

            commandNumber = 2;
            for (const Item& ingredient : listeIngredients){
                Item ingredientNewQuantity = ingredient;
                ingredientNewQuantity.quantity *= quantity;
                write(write_fd, &commandNumber, sizeof(commandNumber));
                write(write_fd, &ingredientNewQuantity, sizeof(ingredient));
                char response[4];
                read(read_fd, &response, sizeof(response));
                if (strcmp(response, "KO\n") == 0){
                    return "KO";
                }               
            }
            return "OK";
        }

        void showStock(){
            std::lock_guard<std::mutex> guard(mutex);
            int commandNumber = 4;
            write(write_fd, &commandNumber, sizeof(commandNumber));
        }

        void closePipes(){
            std::lock_guard<std::mutex> guard(mutex);
            close(write_fd);
            close(read_fd);
        }

        std::string getMenu(){
            std::lock_guard<std::mutex> guard(mutex);
            std::string menuStr = "OK \"Menu";
            int i = 1;
            for (const Reciept& recette : ListeRecettes){
                menuStr += "\\n" + std::to_string(i) + ".";
                menuStr += recette.nom ;
                i++;
            }
            menuStr += "\"\n";
            return menuStr;

        }
};


struct clientArgs{
    int client_fd;
    Cuisine * kitchen;
};



class Serveur{
    Cuisine& cuisine;
    int read_fd;
    int server_fd = -1;
    int port;
    
    public:
        Serveur(Cuisine& c,int p):cuisine(c), port(p){}
        void run();
        void stop(){
            close(read_fd);
            close(server_fd);
        };
    
};


void * clientHandler(void * args){
    clientArgs * client = static_cast<clientArgs*>(args);
    char buffer[256];
    int size = read(client->client_fd, &buffer, sizeof(buffer)-1);
    if (size <= 0){
        close(client->client_fd);
        delete client;
        return nullptr;
    }
    buffer[size] = '\0';

    std::string command = static_cast<std::string>(buffer);
    std::istringstream iss(command);
    std::string a, b, extra;
    iss >> a;
    iss >> b;
    if (a != "GET"){
        char response[] = "KO\n";
        write(client->client_fd, &response, 3);
    }
    else{
        if (b == "menu"){
            if (iss >> extra){
                char response[] = "KO\n";
                write(client->client_fd, &response, 3);
            }
            else{
                std::string menu;
                menu = client->kitchen->getMenu();
                write(client->client_fd, menu.c_str(), menu.size());
                
            }
        }
        else if (b == "plat"){
            std::string idStr, qtyStr, extra;
            int id = 0, qty = 1;
            bool valide = true;

            if (!(iss>> idStr)){
                valide = false;
            }
            else{
                try{
                    size_t pos = 0;
                    id = std::stoi(idStr, &pos);
                    if (pos != idStr.size()) valide = false;
                }
                catch (const std::exception&){
                    valide = false;
                }
            }
            if (valide && (iss >> qtyStr)){
                try{
                    size_t pos =0;
                    qty = std::stoi(qtyStr, &pos);
                    if (pos != qtyStr.size() || qty <=0) valide = false;
                }
                catch(const std::exception&){
                    valide = false;
                }
            }
            if (valide && (iss >> extra)) valide = false;

            if (valide){
                std::string response = client->kitchen->orderItem(id, qty);
                if (response == "KO"){
                    char response[] = "KO\n";
                    write(client->client_fd,&response, 3);
                }
                else if (response == "EPUISE"){
                    response = "EPUISE " + std::to_string(id) + "\n";
                    write(client->client_fd, response.c_str(), response.size());
                }
                else{
                    response = "OK " + std::to_string(id) + " " + std::to_string(qty) + "\n";
                    write(client->client_fd, response.c_str(), response.size());
                }
            }
            else{
                char response[] = "KO\n";
                write(client->client_fd, &response, sizeof(response));
            }

        }
        else{
            char response[] = "KO\n";
            write(client->client_fd, &response, 3);
        }
    }
    close(client->client_fd);
    delete client;
    return nullptr;
};



void Serveur::run(){
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0){
        perror("socket");
        return;
    }

    int opt = 1;

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0){
        perror("bind");
        return;
    }

    if (listen(server_fd, 3) < 0){
        perror("listen");
        return;
    }
    size_t addrlen = sizeof(address);
    while (!arret)
    {
        int client_fd;
        client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if (client_fd < 0){
            perror("accept");
            continue;
        }
        if(arret){
            close(client_fd);
            break;
        }

        pthread_t clientThread;
        clientArgs * client = new clientArgs;
        client->client_fd = client_fd;
        client->kitchen = &cuisine;
        if (pthread_create(&clientThread, NULL, clientHandler, client) != 0){
            delete client;
            close(client_fd);
        }
        else{
            pthread_detach(clientThread);
        }

    }
    close(server_fd);
}




void * serveur(void * arg){
    Cuisine* kitchen = static_cast<Cuisine*>(arg);
    Serveur server(*kitchen, port);
    server.run();


    return nullptr;
};

void reveillerServeur(){
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0){
        perror("socket");
        return;
    }
    struct sockaddr_in servr_addr;
    memset(&servr_addr, 0, sizeof(servr_addr));
    servr_addr.sin_family = AF_INET;
    servr_addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &servr_addr.sin_addr);
    if (connect(sock, (struct sockaddr*)&servr_addr, sizeof(servr_addr)) < 0){
        perror("connect");
        close(sock);
        return;
    }
    close(sock);
}


void * lectureStdin(void * arg){
    Cuisine * cuisine = static_cast<Cuisine*>(arg);
    std::string filename;
    while (std::getline(std::cin, filename))
    {
        Reciept recette;
        recette.nom = std::filesystem::path(filename).stem().string();
        std::vector <Item> items = readCsv(filename);
        //verifier si items contient bien quelque chose !
        if (items.empty()){
            continue;
        }
        recette.ingredients = items;
        cuisine->addReciept(recette);

    }
    return nullptr;
}

void cuisine(int fd_pere_fils, int fd_fils_pere, pid_t stockPid){
    //On lance un thread qui va exécuter le serveur
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, &set, nullptr);
    Cuisine cuisine(fd_pere_fils, fd_fils_pere);
    pthread_t serverThread, stdinThread;
    pthread_create(&serverThread, NULL, serveur, &cuisine);
    pthread_create(&stdinThread, NULL, lectureStdin, &cuisine);

    while (true)
    {
        int sig;
        sigwait(&set, &sig);
        if (sig == SIGUSR1){
            cuisine.showStock();
        }
        else if (sig == SIGINT){
            break;
        }
    }

    arret = true;
    reveillerServeur();
    pthread_join(serverThread, NULL);
    pthread_cancel(stdinThread);
    pthread_join(stdinThread, NULL);
    cuisine.closePipes();
    waitpid(stockPid, 0, 0);
    
}




int main(){
    int fd_pere_fils[2], fd_fils_pere[2];
    if (pipe(fd_pere_fils) < 0){
        perror("pipe");
        return 1;
    }
    if(pipe(fd_fils_pere) < 0){
        perror("pipe");
        return 1;
    }

    pid_t forkValue = fork();
    if (forkValue > 0){
        //parent
        signal(SIGPIPE, SIG_IGN);
        close(fd_pere_fils[READ]);
        close(fd_fils_pere[WRITE]);
        cuisine(fd_pere_fils[WRITE], fd_fils_pere[READ], forkValue);
    }
    else if (forkValue < 0){
        //error => can't make child process
        perror("fork");
        return 1;
    }
    else{
        //child => stock
        close(fd_pere_fils[WRITE]);
        close(fd_fils_pere[READ]);
        stock(fd_pere_fils[READ], fd_fils_pere[WRITE]);
    }


    return 0;
}