// Kolica GPS — šalje obavijesti s tvog Gmaila.
// Mail stiže na Gmail račun koji je objavio ovu skriptu.
// Želiš li obavijesti na neku drugu adresu, upiši je ovdje:
var PRIMATELJ = "";

function doPost(e) {
  try {
    var p = e ? e.parameter : {};
    var naslov = String(p.naslov || "Kolica GPS").slice(0, 200);
    var poruka = String(p.poruka || "").slice(0, 5000);
    var primatelj = PRIMATELJ || Session.getEffectiveUser().getEmail();
    MailApp.sendEmail(primatelj, naslov, poruka);
    return odgovor({ ok: true, primatelj: primatelj });
  } catch (err) {
    return odgovor({ ok: false, greska: String(err) });
  }
}

// Provjera: otvori adresu skripte u pregledniku, treba pisati "ok": true.
function doGet() {
  return odgovor({ ok: true, poruka: "Kolica GPS skripta radi." });
}

function odgovor(podaci) {
  return ContentService.createTextOutput(JSON.stringify(podaci))
    .setMimeType(ContentService.MimeType.JSON);
}
