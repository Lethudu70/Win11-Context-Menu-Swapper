# Win11 Context Menu Swapper — v4.0

Retrouvez le menu contextuel classique ou rétablissez le comportement Windows par défaut depuis une interface Qt 6 arrondie, disponible en français comme en anglais.

**[Télécharger la version portable V4.0](https://github.com/Lethudu70/Win11-Context-Menu-Swapper/releases/tag/V4.0)** · [Documentation et compilation](V4/README.md) · [Versions précédentes](https://github.com/Lethudu70/Win11-Context-Menu-Swapper/releases)

![Interface de Context Menu Swapper v4.0](docs/images/v4.0-main.png)

## Les nouveautés

- **Fenêtre Qt 6 sans bordure Windows**, coins arrondis, boutons réduire/fermer personnalisés et icône intégrée.
- **Windows détecté automatiquement** : édition, version, build complète et architecture native.
- **Changements vérifiés** : état du registre affiché, sauvegarde avant modification et contrôle après écriture.
- **Annulation persistante**, avec restauration de l'état précédent et détection des changements effectués par un autre outil.
- **Redémarrage d'Explorer à la demande**, quand vous choisissez de l'effectuer.
- **Interface FR/EN**, thèmes sombre/système, cases à cocher contrastées et diagnostic exportable.
- **Nettoyage JSON ciblé**, avec conservation des fichiers inconnus et archivage de l'historique actif si vous le réinitialisez.
- **Mode HKLM expérimental séparé**, avec UAC et confirmation explicite.

## Installer et utiliser

1. Téléchargez `ContextMenuSwapper-v4.0-win64-portable.zip` depuis la release V4.0.
2. Extrayez **tout le ZIP** dans un dossier de votre choix.
3. Lancez `ContextMenuSwapper.exe`. Conservez les DLL et les sous-dossiers, notamment `platforms`.
4. Choisissez le menu classique ou le comportement Windows par défaut, puis cliquez sur **Appliquer la sélection**.
5. Redémarrez Explorer quand vous le souhaitez, puis testez le clic droit.

Le portable fournit les bibliothèques nécessaires : Python, Qt et MSYS2 ne sont pas requis sur le PC destinataire. Le mode standard cible votre compte (**HKCU**) et ne demande pas les droits administrateur. Les préférences et l'historique restent dans le profil Windows de l'utilisateur.

## HKLM : réservé aux essais avancés

**EXPÉRIMENTAL — NON RECOMMANDÉ AUX UTILISATEURS STANDARD.** L'option est décochée à chaque lancement. HKLM peut affecter tous les comptes du PC ; un réglage HKCU peut rester prioritaire.

Appliquer ou restaurer en HKLM lance un processus administrateur via l'UAC, puis demande une confirmation. Si Windows protège la valeur, une seconde confirmation peut autoriser une tentative avec le privilège de restauration Windows. Le programme conserve le propriétaire et les permissions de la clé. La sauvegarde machine est distincte de l'historique HKCU.

L'effet de l'override dépend de la build de Windows et des politiques du PC. Une écriture vérifiée dans le registre ne garantit pas l'effet visuel du menu. Testez HKLM dans Windows Sandbox ou dans une VM avant tout usage sur un système principal. Voir la [documentation complète](V4/README.md) et l'[aperçu du mode expérimental](docs/images/v4.0-hklm.png).

## Compiler soi-même

Le code de la v4.0 se trouve dans `V4/`. Il utilise **C++17, Qt 6 Widgets, MSYS2 UCRT64, CMake et Ninja**.

```powershell
cd V4
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-ucrt64.ps1
```

Le script compile, exécute les tests, rassemble Qt et les DLL UCRT64 nécessaires, puis produit un ZIP portable. Les [instructions détaillées](V4/README.md) expliquent les paquets à installer et les options de compilation. GitHub Actions effectue également la compilation et le packaging sous Windows.

## Validation et anciennes versions

Les tests couvrent les sauvegardes, annulations, refus d'accès, permissions conservées, conflits externes et nettoyages JSON. Les essais HKLM automatisés redirigent la ruche vers une clé temporaire ; ils ne modifient pas le menu de la machine de compilation. Les vérifications visuelles sur chaque build Windows restent nécessaires. Les exécutables ne sont pas signés.

La V3 C++ et la version Python historique sont conservées dans le dépôt et les anciennes releases. Pour la nouvelle version C++/Qt 6, utilisez `V4/`.

## English

Context Menu Swapper v4.0 is a portable C++/Qt 6 tool for switching the Windows context-menu configuration. It includes a rounded frameless window, French/English UI, Windows detection, verified registry changes, persistent undo and optional Explorer restart. The default mode changes only the current user's HKCU settings. The separate HKLM mode is experimental, requires administrator authorization and is not recommended for standard users. Extract the entire portable ZIP, including its DLLs and Qt plugin folders.
