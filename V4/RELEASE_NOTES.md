# Context Menu Swapper V4.0 — Leth Edition

Une nouvelle version en **C++ et Qt 6** pour retrouver le menu contextuel classique ou rétablir le comportement Windows par défaut, avec une interface simple et des changements vérifiés.

## Nouveautés

- Fenêtre arrondie sans bordure Windows, boutons réduire/fermer personnalisés et icône intégrée.
- Interface français/anglais, thèmes sombre/système et cases à cocher contrastées.
- Détection de l'édition, version, build complète et architecture Windows.
- Sauvegarde avant modification, contrôle du registre et annulation persistante.
- Détection des changements externes et diagnostic exportable.
- Redémarrage d'Explorer à la demande.
- Nettoyage JSON ciblé avec archivage optionnel des sauvegardes à réinitialiser.
- Mode HKLM expérimental séparé, avec UAC et confirmation explicite. Une confirmation avancée permet de tenter le privilège de restauration Windows sur une valeur protégée, sans changer propriétaire ni DACL.

## Installation

Téléchargez **ContextMenuSwapper-v4.0-win64-portable.zip**, extrayez **tout son contenu**, puis lancez `ContextMenuSwapper.exe`. Conservez les DLL et sous-dossiers Qt, notamment `platforms`. Aucun Python, Qt ou MSYS2 n'est à installer pour utiliser le portable.

`SHA256SUMS.txt` permet de vérifier l'intégrité du ZIP. Les sources et scripts CMake/Ninja/UCRT64 sont disponibles au tag `V4.0`.

## Mode expérimental et compatibilité

**HKCU est le mode standard. HKLM est expérimental, désactivé par défaut et non recommandé aux utilisateurs standard.** Il peut affecter tous les comptes du PC ; HKCU peut rester prioritaire. Testez-le dans Windows Sandbox ou dans une VM.

Le comportement du menu dépend de la build Windows et des politiques système. La vérification du registre ne garantit pas le résultat visuel. Les tests automatisés utilisent des clés isolées ; les essais du menu réel et de l'UAC sur les builds utilisées restent nécessaires. Les exécutables ne sont pas signés.

Documentation : [V4/README.md](https://github.com/Lethudu70/Win11-Context-Menu-Swapper/blob/V4.0/V4/README.md).
