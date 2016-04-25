$(document).ready(function() {
    // Handle console
    var nilFunction = function() {};
    window.console = window.console || {
        log: nilFunction,
        info: nilFunction,
        debug: nilFunction
    };

    $("header").load("header.html");
    
});