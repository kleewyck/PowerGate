$(document).ready(function() {
  // Start 1-second timer to call RESTful endpoint
  setInterval(function() {
    $.ajax({
      url: '/get_cpu_usage',
      dataType: 'json',
      success: function(json) {
	      //console.log(json);
        $('#cpu_usage').text(json.cpu + '% ');
        $('#wifi_status').text(json.status + "\r\n");
        $('#ssid_name').text(json.ssid);
        $('#ip_addr').text(json.ip);
        $('#bvolt').text(json.bvolt);
        $('#psvolt').text(json.psvolt);
        $('#outvolt').text(json.outvolt);
        $('#current').text(json.current);
        $('#relayStatus').text(json.relaysta);
      }
    });

    var relayValue = parseInt($('#relayStatus').text(),10);
    console.log(relayValue);
      if (relayValue == 0){
         $("#relayStatus").removeClass().addClass("lowlight");
      } else {
        $("#relayStatus").removeClass().addClass("highlight");
      }

  }, 1000);

});
