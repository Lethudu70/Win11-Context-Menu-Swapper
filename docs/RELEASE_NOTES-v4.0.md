# Context Menu Swapper V4.0 — Leth Edition

[Français](#français) · [English](#english)

## Français

Une nouvelle version en **C++ et Qt 6** pour retrouver le menu contextuel classique ou rétablir le comportement Windows par défaut, avec une interface simple et des changements vérifiés.

### Nouveautés

- Fenêtre arrondie sans bordure Windows, boutons réduire/fermer personnalisés et icône intégrée.
- Interface français/anglais, thèmes sombre/système et cases à cocher contrastées.
- Détection de l'édition, version, build complète et architecture Windows.
- Sauvegarde avant modification, contrôle du registre et annulation persistante.
- Détection des changements externes et diagnostic exportable.
- Redémarrage d'Explorer à la demande.
- Nettoyage JSON ciblé avec archivage optionnel des sauvegardes à réinitialiser.
- Mode HKLM expérimental séparé, avec UAC et confirmation explicite. Une confirmation avancée permet de tenter le privilège de restauration Windows sur une valeur protégée, sans changer propriétaire ni DACL.

### Installation

Téléchargez **ContextMenuSwapper-v4.0-win64-portable.zip**, extrayez **tout son contenu**, puis lancez `ContextMenuSwapper.exe`. Conservez les DLL et sous-dossiers Qt, notamment `platforms`. Aucun Python, Qt ou MSYS2 n'est à installer pour utiliser le portable.

`SHA256SUMS.txt` permet de vérifier l'intégrité du ZIP. Les sources et scripts CMake/Ninja/UCRT64 sont disponibles au tag `V4.0`.

### Mode expérimental et compatibilité

**HKCU est le mode standard. HKLM est expérimental, désactivé par défaut et non recommandé aux utilisateurs standard.** Il peut affecter tous les comptes du PC ; HKCU peut rester prioritaire. Testez-le dans Windows Sandbox ou dans une VM.

Le comportement du menu dépend de la build Windows et des politiques système. La vérification du registre ne garantit pas le résultat visuel. Les tests automatisés utilisent des clés isolées ; les essais du menu réel et de l'UAC sur les builds utilisées restent nécessaires. Les exécutables ne sont pas signés.

Documentation : [V4/README.md](https://github.com/Lethudu70/Win11-Context-Menu-Swapper/blob/V4.0/V4/README.md).

## English

A new **C++ and Qt 6** release for restoring the classic context menu or returning to the default Windows behavior, with a straightforward interface and verified configuration changes.

### What's new

- Rounded frameless window, custom minimize/close buttons and an embedded application icon.
- French/English interface, dark/system themes and high-contrast checkboxes.
- Automatic detection of the Windows edition, version, full build number and architecture.
- Backup before modification, registry verification and persistent undo.
- Detection of external changes and exportable diagnostics.
- Explorer restart on demand.
- Targeted JSON cleanup with optional archiving of undo history before resetting it.
- Separate experimental HKLM mode with UAC authorization and explicit confirmation. An additional confirmation can authorize an attempt to use the Windows restore privilege on a protected value, without changing the key's owner or DACL.

### Installation

Download **ContextMenuSwapper-v4.0-win64-portable.zip**, extract **all its contents**, then run `ContextMenuSwapper.exe`. Keep the DLLs and Qt subfolders, especially `platforms`. Python, Qt and MSYS2 do not need to be installed to use the portable package.

Use `SHA256SUMS.txt` to verify the ZIP's integrity. Source code and CMake/Ninja/UCRT64 scripts are available at the `V4.0` tag.

### Experimental mode and compatibility

**HKCU is the default mode. HKLM is experimental, disabled by default and not recommended for standard users.** It can affect all accounts on the PC; HKCU may take precedence. Test it in Windows Sandbox or a virtual machine.

Menu behavior depends on the Windows build and system policies. Registry verification does not guarantee the visual result. Automated tests use isolated keys; the real context menu and UAC flow still need to be tested on the Windows builds you use. The executables are unsigned.

Documentation: [V4/README.md (French)](https://github.com/Lethudu70/Win11-Context-Menu-Swapper/blob/V4.0/V4/README.md).
