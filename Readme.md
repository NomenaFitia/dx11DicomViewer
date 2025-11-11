Pour une session :

$env:VCPKG_ROOT = "C:\vcpkg"

Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass

# Avec vcpkg explicite
./build.ps1 -Config Debug -Triplet x64-windows -VcpkgRoot "C:\vcpkg"


# (ou) en utilisant VCPKG_ROOT de la session
$env:VCPKG_ROOT = "C:\vcpkg"
./build.ps1 -Config Debug -Triplet x64-windows

#####################################################

commandes 

- **P** : basculer Perspective ⟷ Orthographic
- **RMB** : rotation (orbite)
- **MMB** : pan
- **Molette** : zoom (dolly / scale ortho)

####################################################

Decimation

Qualité : ce “curvature-aware clustering” produit un maillage propre, manifold-friendly dans la majorité des cas, sans les pièges des collapses (trous / inversions).
Pour plus de finesse, ajuste :

protectTopPercent (ex. 0.05 pour conserver très fin),

factor (plus grand → plus de décimation),

cellScale (affine la taille de cellule).

Normales : on recalcule toujours après décimation (pondération aire).

Decimation os : si tu veux sur-protéger l’os cortical, tu peux passer une version de DecimateByCurvature qui prend un masque de sommets protégés (facile à étendre), ou simplement réduire factor côté os.

Marching Cubes : décime le résultat (maillage) plutôt que le volume, c’est plus stable et plus rapide.

Si tu veux une version edge-collapse (QEM) pondérée par la courbure (coût = QEM * (1 + w * curvature)), je peux te livrer un module séparé — mais pour un viewer DX11, la solution ci-dessus est souvent parfaite en pratique (simple, rapide, robuste) et se branche en 5 minutes.