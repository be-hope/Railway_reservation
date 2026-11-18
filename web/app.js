'use strict';
const $ = selector => document.querySelector(selector);
const esc = value => String(value ?? '').replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
let journey, selected, quote, requestId, cancelPnr, noticeTimer;
const money = value => `₹${esc(value)}`;
function notify(message, error=false) {
  clearTimeout(noticeTimer); $('#notice').textContent=message; $('#notice').className=error?'error':''; $('#notice').hidden=false;
  noticeTimer=setTimeout(()=>$('#notice').hidden=true,8000);
}
async function api(action, body={}) {
  const response=await fetch('/api',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({action,...body})});
  const result=await response.json();
  if (!result.ok) throw new Error(result.error || 'Could not complete this request.');
  return result.data;
}
async function busy(button, task) {
  button.disabled=true;
  try { await task(); } catch(error) { notify(error.message,true); } finally { button.disabled=false; }
}
const formData = form => Object.fromEntries(new FormData(form));
const badge = status => `<span class="badge ${esc(status.toLowerCase())}">${esc(status)}</span>`;
function tab(bookings) {
  $('#search-section').hidden=bookings; $('#bookings-section').hidden=!bookings;
  $('#search-tab').classList.toggle('active',!bookings); $('#bookings-tab').classList.toggle('active',bookings);
}
function signedIn(login) {
  $('#auth').hidden=true; $('#dashboard').hidden=false; $('#logout').hidden=false;
  $('#welcome').textContent=`Travelling as ${login}`; tab(false);
}
$('#login-form').addEventListener('submit',event=>{
  event.preventDefault(); const form=event.currentTarget;
  busy(form.querySelector('button'),async()=>{const result=await api('login',formData(form)); form.reset(); signedIn(result.login);});
});
$('#register-form').addEventListener('submit',event=>{
  event.preventDefault(); const form=event.currentTarget;
  busy(form.querySelector('button'),async()=>{await api('register',formData(form));form.reset();$('#auth-toggle').click();notify('Account created. You can now sign in.');});
});
$('#auth-toggle').onclick=()=>{
  const register=$('#register-form').hidden;
  $('#register-form').hidden=!register;$('#login-form').hidden=register;
  $('#auth-title').textContent=register?'Make yourself at home':'Sign in to Railway';
  $('#auth-caption').textContent=register?'Create your passenger account.':'Your bookings, all in one place.';
  $('#auth-toggle').textContent=register?'Already have an account? Sign in':'New here? Create an account';
};
$('#logout').onclick=()=>busy($('#logout'),async()=>{await api('logout');$('#auth').hidden=false;$('#dashboard').hidden=true;$('#logout').hidden=true;$('#bookings').replaceChildren();$('#results').replaceChildren();});
const stations=[['SBC','Bengaluru City'],['MAS','Chennai Central'],['KPD','Katpadi Jn'],['MYS','Mysuru Jn'],['SC','Secunderabad Jn']];
for(const name of ['source','destination']) $('#search-form').elements[name].innerHTML=stations.map(([code,name])=>`<option value="${code}">${name} (${code})</option>`).join('');
$('#search-form').elements.destination.value='MAS';
const dateInput=$('#search-form').elements.date;
const localDate=d=>`${d.getFullYear()}-${String(d.getMonth()+1).padStart(2,'0')}-${String(d.getDate()).padStart(2,'0')}`;
const today=new Date(), tomorrow=new Date(today);tomorrow.setDate(today.getDate()+1);
const lastDay=new Date(today);lastDay.setDate(today.getDate()+120);
dateInput.min=localDate(today);dateInput.max=localDate(lastDay);dateInput.value=localDate(tomorrow);
$('#search-form').addEventListener('submit',event=>{
  event.preventDefault();const form=event.currentTarget;
  busy(form.querySelector('button'),async()=>{
    const query=formData(form);if(query.source===query.destination)throw new Error('Choose two different stations.');
    const trains=await api('search',query);journey=query;
    $('#results').innerHTML=trains.length?`<div class="result-heading"><span>${trains.length} train${trains.length===1?'':'s'} for your journey</span><span>${esc(query.date)} · ${esc(query.classCode)}</span></div>`:'<div class="empty"><h3>No trains for this journey</h3><p>Try another date or station pair. The demo includes Bengaluru to Chennai.</p></div>';
    for(const train of trains){
      const card=document.createElement('article');card.className='panel train-card';
      card.innerHTML=`<div><h3>${esc(train.name)}</h3><span class="train-number">#${esc(train.number)} · ${esc(query.classCode)}</span></div><div class="times"><div><strong>${esc(train.departure)}</strong><small>${esc(query.source)}</small></div><span class="time-line"></span><div><strong>${esc(train.arrival)}</strong><small>${esc(query.destination)}</small></div></div><div class="fare"><strong>${money(train.fare)}</strong><small>per traveller · ${esc(train.duration)}</small><span class="seats">${train.seats} seats · ${train.rac} RAC</span></div><button class="primary">Select →</button>`;
      card.querySelector('button').onclick=()=>openBooking(train);$('#results').append(card);
    }
  });
});
function travellerData(){return [...document.querySelectorAll('.traveller')].map(field=>Object.fromEntries([...field.querySelectorAll('[data-field]')].map(input=>[input.dataset.field,input.value])));}
function invalidateQuote(){quote=null;$('#confirm-booking').disabled=true;$('#total-fare').textContent='Updating…';}
async function updateQuote(){
  const id=selected.number, count=travellerData().length;invalidateQuote();
  try{
    const result=await api('quote',{...journey,train:id,count});
    if(selected.number!==id || travellerData().length!==count)return;
    quote=result;$('#total-fare').textContent=`₹${quote.fare}`;$('#fare-count').textContent=`${count} traveller${count===1?'':'s'}`;$('#confirm-booking').disabled=false;
  }catch(error){notify(error.message,true);$('#total-fare').textContent='Unavailable';}
}
function addTraveller(){
  const count=travellerData().length;if(count>=6)return;
  const field=document.createElement('fieldset');field.className='traveller';
  field.innerHTML=`<legend>Traveller ${count+1}</legend><div class="traveller-grid"><label>Full name<input data-field="name" required maxlength="100" autocomplete="off"></label><label>Age<input data-field="age" type="number" min="1" max="120" required></label><label>Gender<select data-field="gender"><option>M</option><option>F</option><option>O</option></select></label></div><label class="id-label">ID proof number<input data-field="id" required maxlength="30" autocomplete="off"></label>${count?'<button type="button" class="remove-traveller">Remove traveller</button>':''}`;
  if(count)field.querySelector('button').onclick=()=>{field.remove();renumber();updateQuote();};
  $('#travellers').append(field);renumber();updateQuote();
}
function renumber(){document.querySelectorAll('.traveller legend').forEach((x,i)=>x.textContent=`Traveller ${i+1}`);$('#add-traveller').hidden=travellerData().length>=6;}
function openBooking(train){
  selected=train;requestId=crypto.randomUUID();$('#travellers').replaceChildren();
  $('#booking-title').textContent=train.name;$('#journey-summary').textContent=`${journey.source} → ${journey.destination} · ${journey.date} · ${journey.classCode}`;
  $('#booking-dialog').showModal();addTraveller();
}
$('#add-traveller').onclick=addTraveller;
$('#booking-form').addEventListener('submit',event=>{
  event.preventDefault();if(!quote)return;
  busy($('#confirm-booking'),async()=>{
    const ticket=await api('book',{...journey,train:selected.number,requestId,paise:quote.paise,travellers:travellerData()});
    $('#booking-dialog').close();notify('Your reservation is saved.');showTicket(ticket);
    // Remove the earlier availability snapshot after seats have been allocated.
    $('#results').replaceChildren();
  });
});
function showTicket(ticket){
  $('#ticket-content').innerHTML=`<div class="ticket-pnr">${esc(ticket.pnr)}</div>${badge(ticket.status)}<div class="ticket-route">${esc(ticket.source)} → ${esc(ticket.destination)}</div><div class="ticket-grid"><div><small>Journey date</small>${esc(ticket.date)}</div><div><small>Train / class</small>${esc(ticket.train)} / ${esc(ticket.classCode)}</div><div><small>Fare paid</small>${money(ticket.fare)}</div><div><small>Refund</small>${ticket.refund===null?'—':money(ticket.refund)}</div></div>${ticket.travellers.map(t=>`<div class="passenger-line"><span>${esc(t.name)}</span><span>${esc(t.seat || (ticket.status==='CANCELLED'?'Cancelled':'Seat pending'))}</span></div>`).join('')}`;
  $('#ticket-dialog').showModal();
}
async function loadBookings(){
  const tickets=await api('history');tab(true);$('#bookings').replaceChildren();
  if(!tickets.length)$('#bookings').innerHTML='<div class="empty"><h3>Your first journey starts here</h3><p>Book a train and your ticket will appear here.</p></div>';
  for(const ticket of tickets){
    const card=document.createElement('article');card.className='panel booking-card';
    card.innerHTML=`<div>${badge(ticket.status)}<h3>${esc(ticket.source)} → ${esc(ticket.destination)}</h3><p>${esc(ticket.date)} · Train ${esc(ticket.train)} · ${esc(ticket.classCode)} · PNR ${esc(ticket.pnr)}</p></div><div class="booking-actions"><button class="quiet view-ticket">View ticket</button>${ticket.status==='CANCELLED'?'':'<button class="danger cancel-ticket">Cancel</button>'}</div>`;
    card.querySelector('.view-ticket').onclick=event=>busy(event.currentTarget,async()=>showTicket(await api('ticket',{pnr:ticket.pnr})));
    const cancel=card.querySelector('.cancel-ticket');if(cancel)cancel.onclick=()=>{cancelPnr=ticket.pnr;$('#cancel-dialog').showModal();};
    $('#bookings').append(card);
  }
}
$('#search-tab').onclick=()=>tab(false);
$('#bookings-tab').onclick=()=>busy($('#bookings-tab'),loadBookings);
$('#refresh').onclick=()=>busy($('#refresh'),loadBookings);
$('#keep-ticket').onclick=()=>$('#cancel-dialog').close();
$('#cancel-confirm').onclick=()=>busy($('#cancel-confirm'),async()=>{const ticket=await api('cancel',{pnr:cancelPnr});$('#cancel-dialog').close();await loadBookings();showTicket(ticket);notify(`Ticket cancelled. Refund: ₹${ticket.refund}`);});
document.querySelectorAll('dialog .close').forEach(button=>button.onclick=()=>button.closest('dialog').close());
// Recover an existing HttpOnly session on refresh; no password or token in browser storage.
api('history').then(()=>signedIn('your passenger account')).catch(()=>{});
