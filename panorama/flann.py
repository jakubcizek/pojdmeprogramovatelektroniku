import cv2
obrazky  = [
    cv2.imread("baska1.jpg"),
    cv2.imread("baska2.jpg")
]   
pomer = 600 / obrazky[0].shape[0] 
sirka = int(obrazky[0].shape[1] * pomer) 
zmenseny0 = cv2.resize(obrazky[0], (sirka, 600))
pomer = 600 / obrazky[1].shape[0] 
sirka = int(obrazky[1].shape[1] * pomer) 
zmenseny1 = cv2.resize(obrazky[1], (sirka, 600))
sift = cv2.SIFT_create()
klicovebody0, deskriptory0 = sift.detectAndCompute(zmenseny0, None)
klicovebody1, deskriptory1 = sift.detectAndCompute(zmenseny1, None)
flann = cv2.FlannBasedMatcher({"algorithm":0, "tres": 5}, {"checks": 50})
shody = flann.match(deskriptory0, deskriptory1)
shody = sorted(shody, key=lambda x: x.distance)
obrazek_shody = cv2.drawMatches(
    zmenseny0, klicovebody0, 
    zmenseny1, klicovebody1, 
    shody[:300], None
)
cv2.imwrite(f"baska_shody.jpg", obrazek_shody)
