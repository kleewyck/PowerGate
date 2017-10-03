$(document).ready(function() {
  // Start 1-second timer to call RESTful endpoint
  console.log("Ready Function");
  var relayValue;
  setInterval(function() {
    $.ajax({
      url: '/get_lvd_data',
      dataType: 'json',
      success: function(json) {
	      //console.log(json);
        $('#cpu_usage').text(json.cpu + '% ');
        $('#wifi_status').text(json.status + "\r\n");
        $('#ssid_name').text(json.ssid);
        $('#ip_addr').text(json.ip);
        $('#bvolt').text(json.bvolt/100.0);
        $('#psvolt').text(json.psvolt/100.0);
        $('#outvolt').text(json.outvolt/100.0);
        $('#current').text(json.current/1000.0);
        relayValue = json.relaysta;
      }
    });
    //console.log(relayValue);
      if (relayValue == 0){
         $('#relayStatus').text("Off");
         //$('#relayStatus').removeClass().addClass("lowlight");
      } else {
        $('#relayStatus').text("On");
        //$('#relayStatus').removeClass().addClass("highlight");
      }

  }, 1000);

});
