# Utiliser une image de base Debian
FROM debian:bookworm

# Définir les variables d'environnement pour éviter les questions pendant l'installation
ENV DEBIAN_FRONTEND=noninteractive

# Installer les dépendances nécessaires
RUN apt-get update && apt-get install -y \
    build-essential \
    make \
    g++ \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Copier le script build.sh et le Makefile dans le conteneur
COPY build.sh /usr/src/build.sh
COPY makefile /usr/src/makefile

# Copier tout le contenu du projet dans le conteneur
COPY . /usr/src/

# Définir le répertoire de travail
WORKDIR /usr/src/

# Rendre le script build.sh exécutable
RUN chmod +x /usr/src/build.sh

# Exécuter le script build.sh pour compiler le projet
RUN /usr/src/build.sh

# Exposer le port 8888
EXPOSE 8888

# Commande par défaut pour exécuter l'application
CMD ["./main"]
