import cv2
obrazky  = [
    cv2.imread("baska1.jpg"),
    cv2.imread("baska2.jpg"),
    cv2.imread("baska3.jpg"),
]
skladac = cv2.Stitcher_create()
status, panorama = skladac.stitch(obrazky)
if status == cv2.Stitcher_OK:   
    cv2.imwrite(
        "baska_panorama.jpg", 
        panorama, 
        [cv2.IMWRITE_JPEG_QUALITY, 90]
    )
