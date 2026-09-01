/* SABER × SHIKI — AK-STYLE CONCEPT SITE
   Scroll / nav / reveal / parallax behaviours. */
(function () {
  'use strict';

  const $ = (s, r = document) => r.querySelector(s);
  const $$ = (s, r = document) => Array.from(r.querySelectorAll(s));

  // ---- topbar scrolled state + progress bar ----
  const topbar = $('#topbar');
  const progress = $('#progressbar span');
  const sections = $$('section[id]');
  const navLinks = $$('.topnav__link');
  const parser = new DOMParser();

  function onScroll() {
    const y = window.scrollY;
    topbar.classList.toggle('is-scrolled', y > 20);

    // reading progress
    if (progress) {
      const h = document.documentElement;
      const max = h.scrollHeight - h.clientHeight;
      progress.style.width = (max > 0 ? (y / max) * 100 : 0) + '%';
    }

    // active nav based on which section spans the viewport center
    const probe = y + window.innerHeight * 0.45;
    let active = '#index';
    for (const sec of sections) {
      const r = sec.getBoundingClientRect();
      const top = r.top + y;
      if (probe >= top && probe <= top + r.height) { active = '#' + sec.id; break; }
    }
    navLinks.forEach((l) => {
      l.classList.toggle('is-active', l.getAttribute('href') === active);
    });
  }
  window.addEventListener('scroll', onScroll, { passive: true });
  onScroll();

  // ---- reveal animations ----
  const revealEls = $$('.card, .wcard, .shot, .more__text, .section__head');
  revealEls.forEach((el) => el.classList.add('reveal'));
  const io = new IntersectionObserver((entries) => {
    entries.forEach((e) => {
      if (e.isIntersecting) { e.target.classList.add('is-visible'); io.unobserve(e.target); }
    });
  }, { threshold: 0.12 });
  revealEls.forEach((el) => io.observe(el));

  // ---- parallax watermark & hero bg ----
  const watermarks = $$('.hero__watermark');
  const bgLayers = $$('.hero__bg, .showcase .hero__bg');
  let tick = false;
  window.addEventListener('scroll', () => {
    if (!tick) {
      requestAnimationFrame(() => {
        tick = false;
        const y = window.scrollY;
        const vh = window.innerHeight;
        watermarks.forEach((w) => {
          const r = w.parentElement.getBoundingClientRect();
          const off = (r.top * -0.18) || 0;
          w.style.transform = 'translateX(' + (y * 0.05) + 'px) translateY(' + (off) + 'px)';
        });
        bgLayers.forEach((bg) => {
          const r = bg.parentElement.getBoundingClientRect();
          if (r.bottom > 0 && r.top < vh) {
            const p = (r.top / vh) * -40;
            bg.style.transform = 'translateY(' + p + 'px)';
          }
        });
      });
      tick = true;
    }
  }, { passive: true });
  requestAnimationFrame(() => { tick = false; window.dispatchEvent(new Event('scroll')); });

  // ---- card "展开档案" toggle ----
  $$('.card__more').forEach((btn) => {
    btn.addEventListener('click', () => {
      const text = btn.parentElement.querySelector('.card__text');
      if (!text) return;
      const open = text.classList.toggle('is-open');
      text.style.maxHeight = open ? text.scrollHeight + 'px' : '0';
      text.style.overflow = open ? 'visible' : 'hidden';
      btn.childNodes[0].nodeValue = open ? '收起档案 ' : '展开档案 ';
    });
  });

  // ---- ticker: create a full-width seamless loop by duplicating track ----
  const track = $('.ticker__track');
  if (track) {
    track.innerHTML += track.innerHTML; // double content for seamless -50% loop
  }

  // ---- smooth anchor with topbar offset ----
  $$('a[href^="#"]').forEach((a) => {
    a.addEventListener('click', (e) => {
      const id = a.getAttribute('href');
      if (id.length > 1) {
        const target = $(id);
        if (target) {
          e.preventDefault();
          const top = target.getBoundingClientRect().top + window.scrollY - 70;
          window.scrollTo({ top, behavior: 'smooth' });
        }
      }
    });
  });
})();
