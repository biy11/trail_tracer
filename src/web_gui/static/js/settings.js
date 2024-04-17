
// @version - initial cretaion 
// @version - added sound functions
// @version - Refactored code
// @version - Added event liteners for sound-toggle 


var soundOnOff = false;

document.addEventListener('DOMContentLoaded', function() {
    var settingsButton = document.getElementById('settings-button');
    var settingsMenu = document.getElementById('settings-menu');

    function disableSettingsButton(){
        // Assuming isRecordingActive, trailPlotting, and manualMove are previously defined and their states managed elsewhere
        return isRecordingActive || trailPlotting || manualMove;
    }

    settingsButton.addEventListener('click', function(event) {
        clickSound(); // Play click sound
        if(disableSettingsButton()){
            event.preventDefault(); // Corrected typo here
            console.log('Settings cannot be used at the moment');
        } else {
            // Toggle the display of the settings menu
            settingsMenu.style.display = settingsMenu.style.display === 'block' ? 'none' : 'block';
        }
    });

    // Hide settings menu when clicking anywhere outside the menu
    window.addEventListener('click', function(event) {
        if (!settingsButton.contains(event.target) && !settingsMenu.contains(event.target)) {
            settingsMenu.style.display = 'none';
        }
    });

    // Sound-toggle event listener
    var soundToggle = document.getElementById('sound-toggle');
    soundToggle.addEventListener('click', function() {
        soundOnOff = !soundOnOff;

        this.classList.toggle('active');
        if(this.classList.contains('active')){
            clickSound(); //Play click sound
            loggerLog("Sound On")
            soundToggle.textContent = 'Sound On';
        } else {
            loggerLog("Sound off")
            soundToggle.textContent = 'Sound Off';
        }
    });
});

function clickSound(){
    if(soundOnOff){
        var audio = document.getElementById('click');
        if(audio){
            audio.play();
        }
    }
}

function toggleSound(){
    if(soundOnOff){
        var audio = document.getElementById('toggle_sound');
        if(audio){
            audio.play();
        }
    }   
}

function checkSound(){
    if(soundOnOff){
        var audio = document.getElementById('check_sound');
        if(audio){
            audio.play();
        }
    }
}
