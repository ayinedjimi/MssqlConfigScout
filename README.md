# 🚀 MssqlConfigScout


**Développé par: Ayi NEDJIMI Consultants**

## 📋 Description

MssqlConfigScout est un outil de reconnaissance et d'audit des configurations SQL Server. Il se connecte via ODBC à une instance SQL Server et rapporte les configurations critiques du serveur, notamment :

- Mode d'authentification (Windows Authentication vs Mixed Mode)
- État de xp_cmdshell (procédure stockée permettant l'exécution de commandes système)
- CLR enabled (Common Language Runtime)
- Remote access
- Ole Automation Procedures
- Ad Hoc Distributed Queries
- Database Mail XPs
- Agent XPs
- Et autres extended stored procedures

L'outil permet d'identifier rapidement les configurations potentiellement dangereuses ou non conformes aux bonnes pratiques de sécurité.


## 📌 Prérequis

- Windows 10/11 ou Windows Server 2016+
- Visual Studio 2019+ avec Build Tools (cl.exe)
- SQL Server ODBC Driver installé
- Accès à une instance SQL Server (local ou distant)
- Droits VIEW SERVER STATE sur le serveur SQL


## Compilation

### Option 1 : Utiliser le script batch
```batch
go.bat
```

### Option 2 : Ligne de commande manuelle
```batch
cl.exe /EHsc /W4 /std:c++17 /Fe:MssqlConfigScout.exe MssqlConfigScout.cpp /link comctl32.lib odbc32.lib user32.lib gdi32.lib
```


## 🚀 Utilisation

1. Lancer l'exécutable `MssqlConfigScout.exe`
2. Remplir les champs de connexion :
   - **Serveur** : Nom ou adresse IP du serveur SQL (ex: localhost, 192.168.1.100, SERVEUR\INSTANCE)
   - **Base de données** : Nom de la base (par défaut : master)
   - **Utilisateur** : Login SQL (laisser vide pour Windows Authentication)
   - **Mot de passe** : Mot de passe SQL (laisser vide pour Windows Authentication)
3. Cliquer sur **Connecter et Scanner**
4. Attendre la récupération des configurations
5. Analyser les résultats dans la liste
6. Optionnel : Exporter les résultats en CSV


## Interface

### Champs de saisie
- **Serveur** : Nom du serveur SQL Server
- **Base de données** : Base à laquelle se connecter (master recommandé)
- **Utilisateur** : Login SQL (optionnel si Windows Auth)
- **Mot de passe** : Mot de passe SQL (optionnel si Windows Auth)

### Boutons
- **Connecter et Scanner** : Lance la connexion et le scan des configurations
- **Exporter CSV** : Exporte les résultats au format CSV UTF-8 avec BOM

### Liste des résultats
Colonnes affichées :
- **Nom de Configuration** : Nom du paramètre SQL Server
- **Valeur** : Valeur configurée
- **Valeur par Défaut** : Valeur minimale/par défaut
- **Valeur en Cours** : Valeur actuellement active (value_in_use)
- **Description** : Description du paramètre

### Barre de statut
Affiche l'état actuel de l'opération et le nombre de configurations récupérées.


## ⚙️ Configurations auditées

L'outil vérifie les configurations suivantes dans `sys.configurations` :

1. **xp_cmdshell** : Permet l'exécution de commandes OS (RISQUE ÉLEVÉ si activé)
2. **clr enabled** : Active l'exécution de code .NET dans SQL Server
3. **remote access** : Autorise les connexions distantes
4. **Ole Automation Procedures** : Permet l'automatisation OLE
5. **Ad Hoc Distributed Queries** : Autorise les requêtes distribuées ad-hoc
6. **Database Mail XPs** : Active Database Mail
7. **SMO and DMO XPs** : SQL Management Objects extended procedures
8. **SQL Mail XPs** : Ancien système de messagerie SQL
9. **Agent XPs** : SQL Server Agent extended procedures

L'outil vérifie également :
- **Authentication Mode** : Mode d'authentification du serveur (Windows only ou Mixed)


## Environnement LAB-CONTROLLED

**AVERTISSEMENT CRITIQUE** : Cet outil est exclusivement destiné à un usage dans des environnements de laboratoire contrôlés.

### Utilisations légitimes
- Audit de sécurité autorisé sur vos propres serveurs SQL
- Tests de conformité en environnement de développement
- Recherche académique en sécurité des bases de données
- Formation en cybersécurité

### INTERDICTIONS STRICTES
- Scanner des serveurs SQL sans autorisation écrite explicite
- Utiliser dans un cadre de production sans validation de l'équipe de sécurité
- Partager les résultats sans anonymiser les données sensibles


## Logs

Les logs sont enregistrés dans :
```
%TEMP%\WinTools_MssqlConfigScout_log.txt
```

Les logs contiennent :
- Horodatage de chaque opération
- Tentatives de connexion
- Erreurs de connexion ou d'exécution SQL
- Nombre de configurations récupérées
- Opérations d'export


## Limitations

- Nécessite les drivers ODBC SQL Server installés
- Requiert les droits VIEW SERVER STATE sur le serveur
- Certaines configurations avancées peuvent nécessiter des droits sysadmin
- Ne modifie JAMAIS les configurations (lecture seule)
- L'authentification SQL nécessite que le mode mixte soit activé
- Ne teste pas les connexions à distance (évaluation locale uniquement)


## Interprétation des résultats

### Configurations à risque

**xp_cmdshell = 1** : CRITIQUE
- Permet l'exécution de commandes système depuis SQL
- Vecteur d'attaque majeur si compromis
- Devrait être désactivé sauf besoins métier justifiés

**clr enabled = 1** : MOYEN à ÉLEVÉ
- Permet l'exécution de code .NET
- Peut être utilisé pour escalade de privilèges
- À désactiver si non utilisé

**Ole Automation Procedures = 1** : MOYEN
- Permet l'automatisation COM/OLE
- Potentiel d'abus pour exécution de code
- À désactiver si non utilisé

**Ad Hoc Distributed Queries = 1** : MOYEN
- Autorise OPENROWSET/OPENDATASOURCE
- Peut permettre l'accès à des sources non autorisées
- À désactiver si non utilisé

### Mode d'authentification

**Windows Authentication** : Recommandé
- Plus sécurisé (intégration Active Directory)
- Pas de stockage de mots de passe SQL

**Mixed Mode** : À surveiller
- Permet l'authentification SQL (login/password)
- Risque de mots de passe faibles
- Nécessite une politique de mots de passe forte


## 🔒 Sécurité et Éthique

### Responsabilités de l'utilisateur

1. **Autorisation** : Obtenir une autorisation écrite avant tout scan
2. **Confidentialité** : Ne pas divulguer les vulnérabilités découvertes publiquement
3. **Reporting** : Communiquer les résultats de manière responsable
4. **Légalité** : Respecter toutes les lois locales et internationales

### Bonnes pratiques

- Toujours travailler dans un environnement isolé du réseau de production
- Documenter toutes les actions effectuées
- Ne jamais activer de configurations dangereuses pendant les tests
- Utiliser des comptes de service dédiés avec privilèges minimaux
- Consulter l'équipe de sécurité avant d'utiliser en production

### Clause de non-responsabilité

L'auteur (Ayi NEDJIMI Consultants) et les contributeurs de cet outil déclinent toute responsabilité concernant :
- Les dommages directs ou indirects résultant de l'utilisation de cet outil
- Les utilisations illégales ou non éthiques
- Les pertes de données ou interruptions de service

**L'utilisateur assume l'entière responsabilité légale et éthique de l'utilisation de ce logiciel.**


## Support

Pour toute question ou problème :
- Consulter les logs dans %TEMP%\WinTools_MssqlConfigScout_log.txt
- Vérifier que les drivers ODBC SQL Server sont installés
- Vérifier les permissions sur le serveur SQL
- S'assurer que le serveur SQL autorise les connexions (TCP/IP activé)


## 📄 Licence

Cet outil est fourni "TEL QUEL", sans garantie d'aucune sorte.

**Usage éducatif et professionnel uniquement dans des environnements autorisés.**

- --

**Ayi NEDJIMI Consultants - 2025**


---

<div align="center">

**⭐ Si ce projet vous plaît, n'oubliez pas de lui donner une étoile ! ⭐**

</div>