const char js[] = R"js(
let key_states = {}; // Pomocna struktura pro management stisku klaves rizeni tanku 
let speed = 255;

let socket; // Objekt WebSocket spojeni

const ws_tcp_port = 81; // TCP port websocketoveho spojeni

// Hlavni funkce setup jako v Arduinu. Zavolame na zacatku programu
function setup() {
    //Rucni nastaveni rychlosti
    const dom_speed = document.getElementById("speed");
    dom_speed.addEventListener("change", (event)=>{
        speed = parseInt(event.target.value);
        console.log("Zmena rychlosti na " + speed);
        socket.send("P" + speed);
    });

    // Zaregistrujeme udalosti stisku a uvolneni klaves
    window.addEventListener("keydown", onKeyDown);
    window.addEventListener("keyup", onKeyUp);
    
    // Nastartovani WebSocketu
    socket = new WebSocket("ws://" + window.location.hostname + ":" + ws_tcp_port);
}

// Funkce pro zpracovani stisknuti klavesy a jejiho drzeni
function onKeyDown(event) {
    // Smerove sipky ridi tank
    // Pri stisknuti a drzeni se poslou skrze WebSocket povely FORWARD, BACKWARD, LEFT, RIGHT
    // Aby se pri stisku a drzeni neposilaly zpravy stale dokola, ale jen poprve,
    // v key_states kontrolujeme jestli uz klavesy nahodou nedrzime a v tom pripade z funcke vyskocime
    if (["ArrowUp", "ArrowDown", "ArrowLeft", "ArrowRight"].includes(event.key)) {
        if (key_states[event.key]) {
            return;
        }

        // Ulozime si stav klavesy, kterou prave drzime, pokud jsme ji prave stiskli
        key_states[event.key] = true;

        // Podle klavesy posleme do WebSocket kyzenou zpravu
        switch (event.key) {
            case "ArrowUp":
                socket.send("FORWARD");
                break;
            case "ArrowDown":
                socket.send("BACKWARD");
                break;
            case "ArrowLeft":
                socket.send("LEFT");
                break;
            case "ArrowRight":
                socket.send("RIGHT");
                break;
        }
    }
}

// Funkce pro zpracovani uvoleni klavesy
function onKeyUp(event) {
    // Pokud jsme prave uvolnili nekterou ze smerovych klaves
    if (["ArrowUp", "ArrowDown", "ArrowLeft", "ArrowRight"].includes(event.key)) {
        socket.send("STOP");
        // Zmenime stav klavesy v pomocne promenne key_states 
        key_states[event.key] = false;
    }
}

// Po nacteni stranky zavolame funkci setup
// Fakticky zacatek naseho programu
window.addEventListener("load", setup);
)js";
