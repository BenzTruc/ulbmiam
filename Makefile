CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra
SCRIPT   = afficheur-recettes.sh

all: cuisine commande script

cuisine: cuisine.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ -pthread

commande: commande.cpp
	$(CXX) $(CXXFLAGS) $< -o $@
	
script:
	chmod +x $(SCRIPT)

clean:
	rm -f cuisine commande

.PHONY: all script clean
