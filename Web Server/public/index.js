var form = $(
    "<div><p>What should we call you?</p><form action='greetings' method='POST'><label for='username'></label><input type='text' name='username' autocomplete='off' required></input></form></div>"
);

$(document).ready( () => {
    $("h1").fadeIn(1800);

    setTimeout( () => {
        $("h1").fadeOut(1800);
    }, 3200)
    
    setTimeout( () => {
        $("h1").remove();
    }, 5200)
    
    setTimeout( () => {
        $(".container").append(form);
    }, 6000)
})
