
## Cherokee
Ce projet implémente un serveur de fichiers Web simple sous Linux en utilisant C++11. 
À travers le navigateur, vous pouvez envoyer des requêtes HTTP pour gérer tous les fichiers dans un dossier spécifié sur le serveur. Les principales fonctionnalités incluent :

- Retourner tous les fichiers du dossier sous forme de page HTML
- Possibilité de télécharger des fichiers locaux sur le serveur
- Télécharger les fichiers présents dans la liste
- Supprimer des fichiers spécifiés du serveur

La page de liste de fichiers HTML sert d'interface client, et le serveur retourne la liste des fichiers dans le dossier sous forme de page HTML.

## Architecture générale

- Utilisation du modèle de traitement d'événements Reactor. En unifiant les sources d'événements, le thread principal écoute tous les événements en utilisant epoll, et les threads travailleurs sont responsables du traitement logique des événements.

- Pré-création d'un pool de threads. Lorsqu'un événement se produit, il est ajouté à la file d'attente de travail du pool de threads. Un algorithme de sélection aléatoire choisit un thread du pool pour traiter les événements de la file d'attente.

- Utilisation de la méthode HTTP GET pour obtenir une liste de fichiers et initier des requêtes de téléchargement et de suppression de fichiers. 

- Utilisation de la méthode POST pour télécharger un fichier sur le serveur.
Côté serveur, utilisation d'une machine à états finis pour analyser le message de requête et effectuer l'opération en fonction du résultat de l'analyse, puis envoyer une page, un fichier ou un message de redirection au client.

- Utilisation de la fonction sendfile côté serveur pour implémenter la transmission de données en zero-copy.

## Diagramme de l'architecture
![output (3).png](https://www.dropbox.com/scl/fi/s4ezy7o1vc6wai1ww46ce/output-3.png?rlkey=7r5ok8iz101kkh4ev34oawzfz&dl=0&raw=1)

- **Client Navigateur** : Envoie des requêtes HTTP (GET, POST) au serveur web.
- **Serveur Web** : Reçoit les requêtes HTTP et utilise epoll pour la gestion des événements.
- **Thread Principal** : Écoute les événements et les traite.
- **File d'attente des événements** : Contient les événements entrants à traiter.
- **Pool de Threads** : Contient plusieurs threads de travail qui traitent les événements de la file d'attente.

Les flèches indiquent le flux des événements du client au serveur et entre les composants du serveur. Les threads de travail dans le pool de threads traitent des tâches spécifiques au fur et à mesure que les événements sont traités.
![output (2).png](https://www.dropbox.com/scl/fi/24rhq3ooi13co49vm9qta/output-2.png?rlkey=8afpzm4fnmdtrtcbodjxdxqmk&dl=0&raw=1)
- **Fonctionnalités**
- Obtenir une liste de fichiers
- Upload des fichiers
- Télécharger des fichiers
- Supprimer des fichiers 

Exécution du serveur avec docker 

```
docker-compose up --build
```

Du côté du navigateur, tapez `Server ip(localhost|127.0.0.1):port number(8888)`（Le numéro de port par défaut est 8888） ...
**http://localhost:8888**
- **Si tous les modules sont installés avec c++, l'exécution en local peut être effectuée directement sur la machine avec :**
```sh
chmod +x ./build
./build
./main
```
Du côté du navigateur, tapez : **http://localhost:8888**

