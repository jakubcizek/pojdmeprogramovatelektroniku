import cv2
obrazky  = [
    cv2.imread("baska1.jpg"),
    cv2.imread("baska2.jpg"),
    cv2.imread("baska3.jpg"),
]
sift = cv2.SIFT_create()
for pocitadlo, obrazek in enumerate(obrazky): 
    pomer = 600 / obrazek.shape[0] 
    sirka = int(obrazek.shape[1] * pomer) 
    zmenseny = cv2.resize(obrazek, (sirka, 600))
    klicovacibody, deskriptory = sift.detectAndCompute(zmenseny, None)
    zmenseny = cv2.drawKeypoints(zmenseny, klicovacibody, None)
    cv2.imwrite(
        f"baska_klicovaci_body_{pocitadlo}.jpg", 
        zmenseny, 
        [cv2.IMWRITE_JPEG_QUALITY, 90]
    )
