# Win11 Context Menu Swapper V4.1 — Windows 11 26H2

## Français

- Le mode classique configure désormais les deux CLSID dans HKCU, dont le nouveau `{51668040-5e31-4023-bbe3-149163bd80f1}`. Ce contournement communautaire vise Windows 11 26H2 à partir du build **26300.9818**, dont **26300.9974**. L’auteur a confirmé la manipulation HKCU sur un HP en 26300.9818.
- Le mode HKLM expérimental utilise les deux CLSID avec élévation UAC et confirmation. Il est déconseillé aux utilisateurs standard.
- Les deux clés sont sauvegardées et vérifiées ensemble. **Annuler** peut aussi lire les sauvegardes V4.0 à une clé. Redémarrez Explorer depuis l’application après avoir appliqué.
- **Setup.exe** installe pour votre compte. Français par défaut, anglais au choix. Pour le **ZIP portable**, extrayez tous les fichiers et lancez `ContextMenuSwapper.exe`. Les DLL et plugins Qt sont inclus.

### Configuration de test

| Composant | Configuration |
| --- | --- |
| Machine | HP ProDesk 600 G1 SFF, nom d’hôte `LETH11-PRO` |
| Processeur | Intel Core i3-4170, Haswell Refresh (4e génération), 3,70 GHz, 2 cœurs / 4 threads, x86-64 avec AVX2 |
| Graphiques et sortie vidéo | Intel HD Graphics 4400, VGA |
| Mémoire | 16 Go DDR3 (2 × 8 Go) |
| Stockage | C: SSD 120 Go, D: HDD 500 Go |
| BIOS et démarrage | HP L01 v02.33 (2014), UEFI, Secure Boot désactivé, TPM 1.2 |
| Système testé | Windows 11 Pro 26H2, build **26300.9818** |
| Mise à jour | Build **26300.9974** en cours d’installation au moment du retour utilisateur |

Windows 11 a été installé via Rufus avec contournement des exigences matérielles. La configuration est hors des exigences officielles de Windows 11. Le test manuel sur ce PC confirme le retour au menu classique avec la manipulation HKCU sur 26300.9818. Le résultat visuel sous 26300.9974 et celui du mode HKLM restent à confirmer sur cette machine.

## English

- Classic mode now configures both HKCU CLSIDs, including `{51668040-5e31-4023-bbe3-149163bd80f1}`. This community workaround targets Windows 11 26H2 from build **26300.9818**, including **26300.9974**. The author confirmed the HKCU workaround on an HP running 26300.9818.
- Experimental HKLM mode uses both CLSIDs with UAC elevation and confirmation. Standard users should use HKCU.
- Both keys are backed up and verified together. **Undo** also reads V4.0 single-key backups. Restart Explorer from the app after applying.
- **Setup.exe** installs for your account. French is selected by default, with English available. For the **portable ZIP**, extract all files and launch `ContextMenuSwapper.exe`. Qt DLLs and plugins are included.

### Test system

| Component | Configuration |
| --- | --- |
| PC | HP ProDesk 600 G1 SFF, hostname `LETH11-PRO` |
| CPU | Intel Core i3-4170, Haswell Refresh (4th generation), 3.70 GHz, 2 cores / 4 threads, x86-64 with AVX2 |
| Graphics and video output | Intel HD Graphics 4400, VGA |
| Memory | 16 GB DDR3 (2 × 8 GB) |
| Storage | C: 120 GB SSD, D: 500 GB HDD |
| Firmware and boot | HP L01 v02.33 (2014), UEFI, Secure Boot disabled, TPM 1.2 |
| Tested OS | Windows 11 Pro 26H2, build **26300.9818** |
| Update | Build **26300.9974** was still installing when the test was reported |

Windows 11 was installed with Rufus using its hardware requirement bypass. This configuration falls outside the official Windows 11 requirements. The manual test on this PC confirmed classic menu behavior with the HKCU workaround on 26300.9818. Visual behavior on 26300.9974 and in HKLM mode is pending confirmation on this PC.

The automated tests verify registry writes and restoration using isolated keys. The actual Explorer menu can vary by Windows build and rollout.

[Microsoft Windows 11 build history](https://learn.microsoft.com/en-us/windows/release-health/windows11-release-information) · [Microsoft Explorer context menu notes](https://learn.microsoft.com/windows-insider/release-notes/release-preview-24h2-25h2/build-26200-9807-26300-9807) · [Community report](https://github.com/memstechtips/Winhance/issues/814)
