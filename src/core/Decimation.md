Decimation

Qualité : ce “curvature-aware clustering” produit un maillage propre, manifold-friendly dans la majorité des cas, sans les pièges des collapses (trous / inversions).
Pour plus de finesse, ajuste :

protectTopPercent (ex. 0.05 pour conserver très fin),

factor (plus grand → plus de décimation),

cellScale (affine la taille de cellule).

Normales : on recalcule toujours après décimation (pondération aire).

Decimation os : si tu veux sur-protéger l’os cortical, tu peux passer une version de DecimateByCurvature qui prend un masque de sommets protégés (facile à étendre), ou simplement réduire factor côté os.

Marching Cubes : décime le résultat (maillage) plutôt que le volume, c’est plus stable et plus rapide.
