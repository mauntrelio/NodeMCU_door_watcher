
// connect to events source
var source = new EventSource("/events");

// update the gui based on status information
function updateGUI(status) {

  console.log(status);

  const doorImage = document.getElementById("doorImage");
  const alarmImage = document.getElementById("alarmImage");
  const doorImageWidth = doorImage.width; 

  // update alarm
  alarmImage.src = `alarm_${status.a}.png`;

  // update door
  const containerWidth = document.getElementById("door").offsetWidth;
  if (status.d === 1) { // door open
    doorImage.style.left = `-${doorImageWidth - containerWidth}px`; // Show the right part
  } else { // door closed
    doorImage.style.left = "0px"; // Show the left part
  }
  
}

// update data at start and every 3 seconds
function updateData() {
  var d = new Date();
  fetch("data?t="+d.getTime()).then(function(response){
    if (response.ok) {
      return response.json();
    } else {
      return null;
    }
  }).then(function(json){
    if (json) {
      updateGUI(json);
    }
    setTimeout(updateData,30000);
  });
}

updateData();

// listen to update status events
source.addEventListener("status", function(e) {
  json = JSON.parse(e.data);
  updateGUI(json);
}, false);


