Segmentation (par HU) → labels (0/1/2/3)

Principe : HU = raw * RescaleSlope + RescaleIntercept.
On applique une catégorisation par défaut issue de la littérature :

Os : trabéculaire/cortical ≈ ≥ 300 HU (trabéculaire ~300–800 HU, cortical souvent >1000 HU). 
Radiopaedia
ScienceDirect
Wikipedia

Tissus mous (muscle/organes) : ~ +25 à +100 HU (souvent ~45–50 HU pour muscle/foie non injecté), on élargit un peu [−29, +150] HU pour capturer variabilité et contraste modéré. 
Radiopaedia
uvm.edu

Graisse : ~ −120 à −90 HU (souvent −190 à −30 selon organes/études) → on retient [−190, −30] HU. 
Radiopaedia
Wikipedia
kjronline.org

Air (≈ −1000 HU), liquides/eau (≈ −10 à +15 HU, eau 0) → ignorés (label 0). 
Radiopaedia
Wikipedia

------------------------------------------------------

NB : Les HU varient selon kV, filtres et reconstructions ; gardons ces seuils paramétrables pour affiner plus tard par série si besoin. 
Wikipedia