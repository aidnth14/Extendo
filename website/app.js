/**
 * CONSOLE UI TASTE — INTERACTIVE DASHBOARD & SPEC ENGINE
 * Strictly implementing the 10 rules from console_ui_taste.md
 */

(function () {
  'use strict';

  // =========================================================================
  // 1. SOUND SYNTHESIZER (Web Audio API)
  // Low-latency, authentic micro-sounds for cursor, selection, and transitions.
  // =========================================================================
  class ConsoleAudioEngine {
    constructor() {
      this.ctx = null;
      this.muted = false;
      this.volume = 0.6;
    }

    init() {
      if (!this.ctx) {
        const AudioContext = window.AudioContext || window.webkitAudioContext;
        this.ctx = new AudioContext();
      }
      if (this.ctx.state === 'suspended') {
        this.ctx.resume();
      }
    }

    playNav() {
      if (this.muted) return;
      this.init();
      const now = this.ctx.currentTime;
      const osc = this.ctx.createOscillator();
      const gain = this.ctx.createGain();

      osc.type = 'sine';
      osc.frequency.setValueAtTime(1200, now);
      osc.frequency.exponentialRampToValueAtTime(700, now + 0.035);

      gain.gain.setValueAtTime(0.08 * this.volume, now);
      gain.gain.exponentialRampToValueAtTime(0.001, now + 0.035);

      osc.connect(gain);
      gain.connect(this.ctx.destination);

      osc.start(now);
      osc.stop(now + 0.035);
    }

    playSelect() {
      if (this.muted) return;
      this.init();
      const now = this.ctx.currentTime;
      // Rising warm chime
      const osc1 = this.ctx.createOscillator();
      const osc2 = this.ctx.createOscillator();
      const gain = this.ctx.createGain();

      osc1.type = 'triangle';
      osc2.type = 'sine';
      osc1.frequency.setValueAtTime(587.33, now); // D5
      osc1.frequency.exponentialRampToValueAtTime(880.00, now + 0.09); // A5

      osc2.frequency.setValueAtTime(880.00, now);
      osc2.frequency.exponentialRampToValueAtTime(1174.66, now + 0.12); // D6

      gain.gain.setValueAtTime(0.12 * this.volume, now);
      gain.gain.exponentialRampToValueAtTime(0.001, now + 0.14);

      osc1.connect(gain);
      osc2.connect(gain);
      gain.connect(this.ctx.destination);

      osc1.start(now);
      osc2.start(now);
      osc1.stop(now + 0.14);
      osc2.stop(now + 0.14);
    }

    playBack() {
      if (this.muted) return;
      this.init();
      const now = this.ctx.currentTime;
      const osc = this.ctx.createOscillator();
      const gain = this.ctx.createGain();

      osc.type = 'sine';
      osc.frequency.setValueAtTime(440, now);
      osc.frequency.exponentialRampToValueAtTime(260, now + 0.08);

      gain.gain.setValueAtTime(0.1 * this.volume, now);
      gain.gain.exponentialRampToValueAtTime(0.001, now + 0.08);

      osc.connect(gain);
      gain.connect(this.ctx.destination);

      osc.start(now);
      osc.stop(now + 0.08);
    }

    playLaunch() {
      if (this.muted) return;
      this.init();
      const notes = [440, 554.37, 659.25, 880]; // A major chord
      notes.forEach((freq, idx) => {
        const now = this.ctx.currentTime + idx * 0.06;
        const osc = this.ctx.createOscillator();
        const gain = this.ctx.createGain();

        osc.type = 'sine';
        osc.frequency.setValueAtTime(freq, now);

        gain.gain.setValueAtTime(0.12 * this.volume, now);
        gain.gain.exponentialRampToValueAtTime(0.001, now + 0.28);

        osc.connect(gain);
        gain.connect(this.ctx.destination);

        osc.start(now);
        osc.stop(now + 0.28);
      });
    }

    toggleMute() {
      this.muted = !this.muted;
      return this.muted;
    }
  }

  const audio = new ConsoleAudioEngine();

  // =========================================================================
  // 2. 3DS GAMES LIBRARY (Full-Colour Content Art, Rule 5)
  // High quality vector SVG box arts and metadata
  // =========================================================================
  const GAMES_LIBRARY = [
    {
      id: 'zelda-albw',
      title: 'The Legend of Zelda: A Link Between Worlds',
      shortTitle: 'Zelda: ALBW',
      category: 'action',
      publisher: 'Nintendo',
      developer: 'Nintendo EPD',
      genre: 'Action-Adventure',
      rating: '★ 9.4',
      playtime: '24 hrs 18 min',
      saveStatus: 'Slot 1 • Lorule Castle',
      summary: 'A daring return to the realm of Hyrule and the dark parallel world of Lorule. Transform into living wall graffiti to solve dimensional puzzles.',
      artGradient: 'linear-gradient(135deg, #15803d, #047857 60%, #064e3b)',
      artSymbol: '▲\n▲ ▲',
      badgeColor: '#eab308',
      accentColor: '#10b981'
    },
    {
      id: 'mario-3d-land',
      title: 'Super Mario 3D Land',
      shortTitle: 'Super Mario 3D Land',
      category: 'platformer',
      publisher: 'Nintendo',
      developer: 'Nintendo EAD Tokyo',
      genre: '3D Platformer',
      rating: '★ 9.2',
      playtime: '18 hrs 42 min',
      saveStatus: 'Special 8-Crown',
      summary: 'Reinvents 3D Mario platforming with precise perspective jumps, Tanooki tail glides, and vibrant obstacle courses built for stereoscopic depth.',
      artGradient: 'linear-gradient(135deg, #dc2626, #b91c1c 60%, #7f1d1d)',
      artSymbol: 'M',
      badgeColor: '#f97316',
      accentColor: '#ef4444'
    },
    {
      id: 'pokemon-omega-ruby',
      title: 'Pokémon Omega Ruby',
      shortTitle: 'Pokémon Omega Ruby',
      category: 'action',
      publisher: 'The Pokémon Company',
      developer: 'Game Freak',
      genre: 'Monster Tamer RPG',
      rating: '★ 9.0',
      playtime: '62 hrs 10 min',
      saveStatus: 'Battle Resort • Hall of Fame',
      summary: 'Embark on an epic adventure through the Hoenn region. Witness Primal Reversion for Groudon and soar on the back of Mega Latios.',
      artGradient: 'linear-gradient(135deg, #b91c1c, #991b1b 60%, #450a0a)',
      artSymbol: 'Ω',
      badgeColor: '#ef4444',
      accentColor: '#dc2626'
    },
    {
      id: 'animal-crossing-new-leaf',
      title: 'Animal Crossing: New Leaf',
      shortTitle: 'Animal Crossing: New Leaf',
      category: 'favorites',
      publisher: 'Nintendo',
      developer: 'Nintendo EAD',
      genre: 'Life Simulation',
      rating: '★ 9.3',
      playtime: '142 hrs 05 min',
      saveStatus: 'Town: Belltree • Perfect Town',
      summary: 'Step off the train and take office as Mayor! Enact town ordinances, build public works projects, and relax with friendly animal neighbors.',
      artGradient: 'linear-gradient(135deg, #059669, #047857 60%, #064e3b)',
      artSymbol: '🍃',
      badgeColor: '#10b981',
      accentColor: '#10b981'
    },
    {
      id: 'metroid-samus-returns',
      title: 'Metroid: Samus Returns',
      shortTitle: 'Metroid: Samus Returns',
      category: 'action',
      publisher: 'Nintendo',
      developer: 'MercurySteam & Nintendo',
      genre: 'Metroidvania',
      rating: '★ 9.1',
      playtime: '14 hrs 30 min',
      saveStatus: 'Area 6 • 38 Metroids Defeated',
      summary: 'A fierce reimagining of the Game Boy classic. Master the Melee Counter, 360-degree Free Aim, and devastating Aeion abilities on Planet SR388.',
      artGradient: 'linear-gradient(135deg, #7c3aed, #6d28d9 60%, #2e1065)',
      artSymbol: '⚡',
      badgeColor: '#a855f7',
      accentColor: '#8b5cf6'
    },
    {
      id: 'mario-kart-7',
      title: 'Mario Kart 7',
      shortTitle: 'Mario Kart 7',
      category: 'platformer',
      publisher: 'Nintendo',
      developer: 'Nintendo EAD / Retro Studios',
      genre: 'Arcade Racing',
      rating: '★ 8.9',
      playtime: '38 hrs 15 min',
      saveStatus: '150cc Gold Trophies All ★★★',
      summary: 'Kart racing takes flight with hang gliders and dives underwater with propellers. Custom kart frames and first-person gyro steering.',
      artGradient: 'linear-gradient(135deg, #2563eb, #1d4ed8 60%, #1e3a8a)',
      artSymbol: '7',
      badgeColor: '#3b82f6',
      accentColor: '#3b82f6'
    },
    {
      id: 'fire-emblem-awakening',
      title: 'Fire Emblem: Awakening',
      shortTitle: 'Fire Emblem: Awakening',
      category: 'action',
      publisher: 'Nintendo',
      developer: 'Intelligent Systems',
      genre: 'Tactical RPG',
      rating: '★ 9.5',
      playtime: '74 hrs 50 min',
      saveStatus: 'Chapter 25 • Lunatic Mode',
      summary: 'Command the Shepherds alongside Prince Chrom. Build deep battlefield bonds that pass legendary skills and children down generations.',
      artGradient: 'linear-gradient(135deg, #0284c7, #0369a1 60%, #082f49)',
      artSymbol: '⚔️',
      badgeColor: '#0ea5e9',
      accentColor: '#0284c7'
    },
    {
      id: 'kid-icarus-uprising',
      title: 'Kid Icarus: Uprising',
      shortTitle: 'Kid Icarus: Uprising',
      category: 'action',
      publisher: 'Nintendo',
      developer: 'Project Sora',
      genre: 'Action Shooter',
      rating: '★ 8.8',
      playtime: '29 hrs 12 min',
      saveStatus: 'Chapter 19 • Intensity 9.0',
      summary: 'Pit flies into exhilarating aerial shoot-outs and high-speed ground battles directed by Masahiro Sakurai, packed with witty banter.',
      artGradient: 'linear-gradient(135deg, #d97706, #b45309 60%, #451a03)',
      artSymbol: '🪶',
      badgeColor: '#f59e0b',
      accentColor: '#d97706'
    },
    {
      id: 'kirby-planet-robobot',
      title: 'Kirby: Planet Robobot',
      shortTitle: 'Kirby: Planet Robobot',
      category: 'platformer',
      publisher: 'Nintendo',
      developer: 'HAL Laboratory',
      genre: 'Action Platformer',
      rating: '★ 9.0',
      playtime: '12 hrs 25 min',
      saveStatus: '100% Meta Knightmare Done',
      summary: 'Halt the mechanization of Dream Land! Pilot the mighty Robobot Armor that scans enemies to unleash destructive mechanized copy abilities.',
      artGradient: 'linear-gradient(135deg, #db2777, #be185d 60%, #500724)',
      artSymbol: '🤖',
      badgeColor: '#ec4899',
      accentColor: '#db2777'
    },
    {
      id: 'monster-hunter-4-ultimate',
      title: 'Monster Hunter 4 Ultimate',
      shortTitle: 'Monster Hunter 4U',
      category: 'action',
      publisher: 'Capcom',
      developer: 'Capcom',
      genre: 'Action Hunting RPG',
      rating: '★ 9.2',
      playtime: '185 hrs 40 min',
      saveStatus: 'G-Rank Crown • Charge Blade',
      summary: 'Scale cliffs, mount titanic wyverns, and master the Insect Glaive. The definitive zenith of handheld monster hunting.',
      artGradient: 'linear-gradient(135deg, #9a3412, #7c2d12 60%, #431407)',
      artSymbol: '🐉',
      badgeColor: '#ea580c',
      accentColor: '#c2410c'
    },
    {
      id: 'luigis-mansion-dark-moon',
      title: "Luigi's Mansion: Dark Moon",
      shortTitle: "Luigi's Mansion",
      category: 'favorites',
      publisher: 'Nintendo',
      developer: 'Next Level Games',
      genre: 'Action Adventure',
      rating: '★ 8.7',
      playtime: '16 hrs 10 min',
      saveStatus: 'Haunted Towers • All Boos Found',
      summary: 'Equip the Poltergust 5000 and Dark-Light Device to capture mischievous ghosts across multiple eerie mansions in Evershade Valley.',
      artGradient: 'linear-gradient(135deg, #0d9488, #0f766e 60%, #134e4a)',
      artSymbol: '👻',
      badgeColor: '#14b8a6',
      accentColor: '#0d9488'
    },
    {
      id: 'smash-bros-3ds',
      title: 'Super Smash Bros. for Nintendo 3DS',
      shortTitle: 'Smash Bros. 3DS',
      category: 'favorites',
      publisher: 'Nintendo',
      developer: 'Bandai Namco / Sora',
      genre: 'Fighting / Brawler',
      rating: '★ 9.3',
      playtime: '112 hrs 35 min',
      saveStatus: 'Smash Run • All Fighters Unlocked',
      summary: 'Over 50 gaming icons battle on the go! High-octane 60 FPS combat with stereoscopic depth and the 3DS-exclusive Smash Run dungeon mode.',
      artGradient: 'linear-gradient(135deg, #e11d48, #be123c 60%, #4c0519)',
      artSymbol: '💥',
      badgeColor: '#f43f5e',
      accentColor: '#e11d48'
    }
  ];

  // =========================================================================
  // 3. STATE MANAGEMENT
  // =========================================================================
  const state = {
    currentIndex: 0,
    filteredGames: [...GAMES_LIBRARY],
    currentFilter: 'all',
    activeView: 'dashboard', // 'dashboard' | 'spec'
    focusMode: 'tasteful', // 'tasteful' (scale + inversion) | 'desktop' (outline)
    activeDockIndex: -1, // -1 means shelf is focused; 0..5 means dock button focused
    activeModal: null, // null | 'game' | 'settings' | 'power'
    audioMuted: false,
    controllerLayout: 'nintendo', // 'nintendo' | 'xbox' | 'playstation'
    currentTheme: 'sapphire'
  };

  // DOM Elements
  const shelfTrack = document.getElementById('shelf-track');
  const heroTitle = document.getElementById('hero-title');
  const heroGenre = document.getElementById('hero-genre');
  const heroRating = document.getElementById('hero-rating');
  const heroSummary = document.getElementById('hero-summary');
  const heroPlaytime = document.getElementById('hero-playtime');
  const heroDeveloper = document.getElementById('hero-developer');
  const heroSaveStatus = document.getElementById('hero-savestatus');
  const heroArtBackdrop = document.getElementById('hero-art-backdrop');
  const shelfCounter = document.getElementById('shelf-counter');

  // Modals
  const gameModal = document.getElementById('game-modal');
  const settingsModal = document.getElementById('settings-modal');
  const powerModal = document.getElementById('power-modal');

  // Legend Bar Verbs
  const legendVerbA = document.getElementById('legend-verb-a');
  const legendVerbB = document.getElementById('legend-verb-b');
  const legendVerbX = document.getElementById('legend-verb-x');
  const legendVerbY = document.getElementById('legend-verb-y');

  // Dock Buttons
  const dockButtons = Array.from(document.querySelectorAll('.circular-dock-btn:not(.modal-close-btn)'));

  // =========================================================================
  // 4. RENDERING GAME SHELF & HERO
  // Rule 1 (Scale + Inversion), Rule 3 (Nested radii), Rule 9 (Peeking shelf)
  // =========================================================================
  function renderShelf() {
    shelfTrack.innerHTML = '';
    state.filteredGames.forEach((game, index) => {
      const card = document.createElement('div');
      card.className = `game-card ${index === state.currentIndex ? 'selected' : ''}`;
      card.tabIndex = 0;
      card.dataset.index = index;
      card.setAttribute('role', 'option');
      card.setAttribute('aria-selected', index === state.currentIndex ? 'true' : 'false');

      // Rule 5: Content Artwork is full-colour
      card.innerHTML = `
        <div class="card-thumbnail-wrap">
          <div class="card-thumbnail" style="background: ${game.artGradient}; display:flex; flex-direction:column; align-items:center; justify-content:center; color:#fff;">
            <div style="font-size: 34px; line-height:1; font-weight:800; filter: drop-shadow(0 4px 8px rgba(0,0,0,0.4));">${game.artSymbol}</div>
            <div style="font-size: 11px; font-weight:700; margin-top:10px; opacity:0.85; text-transform:uppercase; letter-spacing:0.5px;">3DS</div>
          </div>
        </div>
        <div class="card-body">
          <h2 class="card-title">${game.shortTitle}</h2>
          <span class="card-sub">${game.genre}</span>
        </div>
      `;

      card.addEventListener('mouseenter', () => {
        if (state.currentIndex !== index) {
          selectGame(index, true);
        }
      });

      card.addEventListener('click', () => {
        selectGame(index, false);
        audio.playSelect();
        openGameModal(game);
      });

      shelfTrack.appendChild(card);
    });

    updateShelfPosition();
    updateHero(state.filteredGames[state.currentIndex]);
  }

  function selectGame(index, playAudio = true) {
    if (index < 0 || index >= state.filteredGames.length) return;
    state.currentIndex = index;
    state.activeDockIndex = -1; // remove dock focus

    dockButtons.forEach(btn => btn.classList.remove('active'));
    document.getElementById('dock-home').classList.add('active');

    // Update selected class on cards
    const cards = shelfTrack.querySelectorAll('.game-card');
    cards.forEach((c, idx) => {
      if (idx === index) {
        c.classList.add('selected');
        c.setAttribute('aria-selected', 'true');
      } else {
        c.classList.remove('selected');
        c.setAttribute('aria-selected', 'false');
      }
    });

    if (playAudio) {
      audio.playNav();
    }

    updateHero(state.filteredGames[index]);
    updateShelfPosition();
    updateLegendContext();
  }

  function updateShelfPosition() {
    // Center the selected card, letting left & right cards peek and breathe (Rule 9)
    const cardWidth = 176 + 24; // card width + gap
    const containerWidth = shelfTrack.parentElement.clientWidth;
    const offset = (containerWidth / 2) - (cardWidth / 2) - (state.currentIndex * cardWidth);
    shelfTrack.style.transform = `translateX(${offset}px)`;

    shelfCounter.textContent = `Tile ${state.currentIndex + 1} of ${state.filteredGames.length}`;
  }

  function updateHero(game) {
    if (!game) return;
    heroTitle.textContent = game.title;
    heroGenre.textContent = game.genre;
    heroRating.textContent = game.rating;
    heroSummary.textContent = game.summary;
    heroPlaytime.textContent = game.playtime;
    heroDeveloper.textContent = game.developer;
    heroSaveStatus.textContent = game.saveStatus;

    // Single hue ambient accent for focused item (Rule 2)
    heroArtBackdrop.style.background = `radial-gradient(ellipse at 85% 40%, ${game.accentColor}25 0%, transparent 65%)`;
  }

  // =========================================================================
  // 5. BUTTON LEGEND CONTEXT (Rule 4)
  // Docked bottom-right, updates verbs depending on state
  // =========================================================================
  function updateLegendContext() {
    if (state.activeModal === 'game') {
      legendVerbA.textContent = 'Play';
      legendVerbB.textContent = 'Close';
      legendVerbX.textContent = 'Save State';
      legendVerbY.textContent = 'Touch Screen';
    } else if (state.activeModal) {
      legendVerbA.textContent = 'Confirm';
      legendVerbB.textContent = 'Close';
      legendVerbX.textContent = 'Default';
      legendVerbY.textContent = 'Help';
    } else if (state.activeView === 'spec') {
      legendVerbA.textContent = 'Toggle Mode';
      legendVerbB.textContent = 'Dashboard';
      legendVerbX.textContent = 'Test Slider';
      legendVerbY.textContent = 'Back';
    } else if (state.activeDockIndex >= 0) {
      legendVerbA.textContent = 'Select Action';
      legendVerbB.textContent = 'To Shelf';
      legendVerbX.textContent = 'Options';
      legendVerbY.textContent = 'Taste Spec';
    } else {
      legendVerbA.textContent = 'Launch';
      legendVerbB.textContent = 'Back';
      legendVerbX.textContent = 'Details';
      legendVerbY.textContent = 'Taste Spec';
    }
  }

  // =========================================================================
  // 6. MODALS MANAGEMENT
  // =========================================================================
  function openGameModal(game) {
    state.activeModal = 'game';
    document.getElementById('modal-game-title').textContent = game.title;
    document.getElementById('modal-game-genre').textContent = game.genre;
    document.getElementById('modal-game-desc').textContent = game.summary;
    document.getElementById('modal-game-pub').textContent = game.publisher;

    // Set top screen 3DS banner
    const banner = document.getElementById('modal-screen-banner');
    banner.style.background = game.artGradient;
    banner.innerHTML = `
      <div style="height:100%; display:flex; flex-direction:column; align-items:center; justify-content:center; color:#fff;">
        <span style="font-size:42px; font-weight:800; filter:drop-shadow(0 4px 12px rgba(0,0,0,0.5))">${game.artSymbol}</span>
        <span style="font-size:13px; font-weight:700; margin-top:8px; letter-spacing:1px;">NINTENDO 3DS™</span>
      </div>
    `;

    gameModal.classList.add('open');
    updateLegendContext();
  }

  function closeModal() {
    if (state.activeModal) {
      audio.playBack();
      document.querySelectorAll('.modal-backdrop').forEach(m => m.classList.remove('open'));
      state.activeModal = null;
      updateLegendContext();
    }
  }

  // Touch screen slot interaction
  document.querySelectorAll('.touch-slot').forEach(slot => {
    slot.addEventListener('click', () => {
      audio.playNav();
      document.querySelectorAll('.touch-slot').forEach(s => s.classList.remove('active'));
      slot.classList.add('active');
    });
  });

  // Launch button simulation
  document.getElementById('btn-launch-game').addEventListener('click', () => {
    audio.playLaunch();
    const btn = document.getElementById('btn-launch-game');
    const original = btn.innerHTML;
    btn.innerHTML = '<span>Booting Core...</span>';
    setTimeout(() => {
      btn.innerHTML = '<span>Emulating 60.0 FPS</span>';
      setTimeout(() => {
        btn.innerHTML = original;
      }, 1500);
    }, 600);
  });

  // Save State button
  document.getElementById('btn-save-state').addEventListener('click', () => {
    audio.playSelect();
    const btn = document.getElementById('btn-save-state');
    const old = btn.innerHTML;
    btn.innerHTML = '<span>Slot 1 Saved!</span>';
    setTimeout(() => { btn.innerHTML = old; }, 1200);
  });

  // Close buttons
  document.getElementById('btn-close-modal').addEventListener('click', closeModal);
  document.getElementById('btn-close-settings').addEventListener('click', closeModal);
  document.getElementById('btn-cancel-power').addEventListener('click', closeModal);
  document.getElementById('btn-sleep').addEventListener('click', () => {
    audio.playBack();
    closeModal();
  });

  // =========================================================================
  // 7. DOCK BUTTON ACTIONS (Rule 6: Circular Icon Buttons, Active inverts)
  // =========================================================================
  dockButtons.forEach((btn, idx) => {
    btn.addEventListener('click', () => {
      audio.playSelect();
      dockButtons.forEach(b => b.classList.remove('active'));
      btn.classList.add('active');
      state.activeDockIndex = idx;

      const action = btn.dataset.action;
      if (action === 'settings') {
        state.activeModal = 'settings';
        settingsModal.classList.add('open');
      } else if (action === 'power') {
        state.activeModal = 'power';
        powerModal.classList.add('open');
      } else if (action === 'theme') {
        state.activeModal = 'settings';
        settingsModal.classList.add('open');
      } else if (action === 'home') {
        switchView('dashboard');
      } else if (action === 'gallery') {
        switchView('spec');
      }
      updateLegendContext();
    });
  });

  // =========================================================================
  // 8. TASTE SPECIFICATION INTERACTIVE DEMOS (The 10 Rules)
  // =========================================================================

  // RULE 1 DEMO: Toggle Scale + Inversion vs Border
  function setFocusMode(mode) {
    state.focusMode = mode;
    document.body.setAttribute('data-focus-mode', mode);

    const btnTasteful = document.getElementById('btn-sel-tasteful');
    const btnDesktop = document.getElementById('btn-sel-desktop');
    const statusText = document.getElementById('rule1-status');

    if (mode === 'tasteful') {
      btnTasteful.classList.add('active');
      btnDesktop.classList.remove('active');
      statusText.textContent = 'Currently: Scale + Inversion (Solid White Flip)';
    } else {
      btnDesktop.classList.add('active');
      btnTasteful.classList.remove('active');
      statusText.textContent = 'Currently: Generic Desktop Outline Border';
    }
  }

  document.getElementById('toggle-focus-mode').addEventListener('click', () => {
    audio.playNav();
    const next = state.focusMode === 'tasteful' ? 'desktop' : 'tasteful';
    setFocusMode(next);
  });

  document.getElementById('btn-test-rule1').addEventListener('click', () => {
    audio.playNav();
    const next = state.focusMode === 'tasteful' ? 'desktop' : 'tasteful';
    setFocusMode(next);
  });

  document.getElementById('btn-sel-tasteful').addEventListener('click', () => {
    audio.playSelect();
    setFocusMode('tasteful');
  });

  document.getElementById('btn-sel-desktop').addEventListener('click', () => {
    audio.playSelect();
    setFocusMode('desktop');
  });

  // RULE 2 DEMO: One Saturated Hue Screen Palette
  function setTheme(theme) {
    state.currentTheme = theme;
    document.body.setAttribute('data-theme', theme);

    // Update active swatch in spec
    document.querySelectorAll('.hue-swatch').forEach(s => {
      s.classList.toggle('active', s.dataset.hue === getThemeHue(theme));
    });

    // Update active theme chip in settings
    document.querySelectorAll('.theme-chip').forEach(c => {
      c.classList.toggle('active', c.textContent.toLowerCase().includes(theme));
    });
  }

  function getThemeHue(theme) {
    switch (theme) {
      case 'cyan': return '188';
      case 'coral': return '352';
      case 'emerald': return '152';
      case 'violet': return '272';
      case 'oled': return '220';
      default: return '214';
    }
  }

  document.querySelectorAll('.hue-swatch').forEach(swatch => {
    swatch.addEventListener('click', () => {
      audio.playSelect();
      const h = swatch.dataset.hue;
      let theme = 'sapphire';
      if (h === '188') theme = 'cyan';
      else if (h === '352') theme = 'coral';
      else if (h === '152') theme = 'emerald';
      else if (h === '272') theme = 'violet';
      else if (h === '220') theme = 'oled';
      setTheme(theme);
    });
  });

  document.querySelectorAll('.theme-chip').forEach(chip => {
    chip.addEventListener('click', () => {
      audio.playSelect();
      const name = chip.dataset.name.toLowerCase();
      let theme = 'sapphire';
      if (name.includes('cyan')) theme = 'cyan';
      else if (name.includes('coral')) theme = 'coral';
      else if (name.includes('jade') || name.includes('emerald')) theme = 'emerald';
      else if (name.includes('violet')) theme = 'violet';
      else if (name.includes('oled')) theme = 'oled';
      setTheme(theme);
    });
  });

  // RULE 7 DEMO: Quiet Status Battery Slider
  const batterySlider = document.getElementById('battery-slider');
  const demoBatteryFill = document.getElementById('demo-battery-fill');
  const demoBatteryVal = document.getElementById('demo-battery-val');
  const headerBatteryFill = document.getElementById('battery-level-fill');
  const headerBatteryPct = document.getElementById('battery-pct-label');

  if (batterySlider) {
    batterySlider.addEventListener('input', (e) => {
      const val = parseInt(e.target.value, 10);
      demoBatteryFill.style.width = `${val}%`;
      demoBatteryVal.textContent = `${val}%`;
      headerBatteryFill.style.width = `${val}%`;
      headerBatteryPct.textContent = `${val}%`;

      if (val <= 15) {
        demoBatteryFill.classList.add('low-charge');
        headerBatteryFill.classList.add('low-charge');
      } else {
        demoBatteryFill.classList.remove('low-charge');
        headerBatteryFill.classList.remove('low-charge');
      }
    });
  }

  // RULE 10 DEMO: Springy Press Button
  const btnSpring = document.getElementById('btn-spring-test');
  if (btnSpring) {
    btnSpring.addEventListener('click', () => {
      audio.playSelect();
    });
  }

  // RULE 4 DEMO: Controller Layout Switcher
  document.querySelectorAll('[data-layout]').forEach(btn => {
    btn.addEventListener('click', () => {
      audio.playSelect();
      document.querySelectorAll('[data-layout]').forEach(b => b.classList.remove('active'));
      btn.classList.add('active');
      state.controllerLayout = btn.dataset.layout;
      applyControllerLayout(state.controllerLayout);
    });
  });

  function applyControllerLayout(layout) {
    const glyphs = document.querySelectorAll('.glyph-badge, .legend-badge');
    if (layout === 'playstation') {
      if (glyphs[0]) glyphs[0].textContent = '✕';
      if (glyphs[1]) glyphs[1].textContent = '○';
      if (glyphs[2]) glyphs[2].textContent = '□';
      if (glyphs[3]) glyphs[3].textContent = '△';
    } else if (layout === 'xbox') {
      if (glyphs[0]) glyphs[0].textContent = 'A';
      if (glyphs[1]) glyphs[1].textContent = 'B';
      if (glyphs[2]) glyphs[2].textContent = 'X';
      if (glyphs[3]) glyphs[3].textContent = 'Y';
    } else {
      if (glyphs[0]) glyphs[0].textContent = 'Ⓐ';
      if (glyphs[1]) glyphs[1].textContent = 'Ⓑ';
      if (glyphs[2]) glyphs[2].textContent = 'Ⓧ';
      if (glyphs[3]) glyphs[3].textContent = 'Ⓨ';
    }
  }

  // =========================================================================
  // 9. VIEW NAVIGATION (Dashboard vs Taste Spec)
  // =========================================================================
  function switchView(viewName) {
    state.activeView = viewName;
    audio.playNav();

    document.querySelectorAll('.view-panel').forEach(p => p.classList.remove('active'));
    document.querySelectorAll('.nav-tab').forEach(t => t.classList.remove('active'));

    if (viewName === 'dashboard') {
      document.getElementById('view-dashboard').classList.add('active');
      document.querySelector('[data-view="dashboard"]').classList.add('active');
      updateShelfPosition();
    } else {
      document.getElementById('view-spec').classList.add('active');
      document.querySelector('[data-view="spec"]').classList.add('active');
    }
    updateLegendContext();
  }

  document.querySelectorAll('.nav-tab').forEach(tab => {
    tab.addEventListener('click', () => {
      switchView(tab.dataset.view);
    });
  });

  // Filter Chips in Shelf
  document.querySelectorAll('.filter-chip').forEach(chip => {
    chip.addEventListener('click', () => {
      audio.playNav();
      document.querySelectorAll('.filter-chip').forEach(c => c.classList.remove('active'));
      chip.classList.add('active');

      const filter = chip.dataset.filter;
      state.currentFilter = filter;
      if (filter === 'all') {
        state.filteredGames = [...GAMES_LIBRARY];
      } else if (filter === 'favorites') {
        state.filteredGames = GAMES_LIBRARY.filter(g => g.category === 'favorites');
      } else {
        state.filteredGames = GAMES_LIBRARY.filter(g => g.category === filter);
      }
      state.currentIndex = 0;
      renderShelf();
    });
  });

  // Profile Header Button
  document.getElementById('btn-profile-header').addEventListener('click', () => {
    audio.playSelect();
    state.activeModal = 'settings';
    settingsModal.classList.add('open');
    updateLegendContext();
  });

  // Audio Toggle Button
  const btnToggleAudio = document.getElementById('btn-toggle-audio');
  const iconAudioOn = btnToggleAudio.querySelector('.icon-audio-on');
  const iconAudioOff = btnToggleAudio.querySelector('.icon-audio-off');

  btnToggleAudio.addEventListener('click', () => {
    const isMuted = audio.toggleMute();
    if (isMuted) {
      iconAudioOn.style.display = 'none';
      iconAudioOff.style.display = 'block';
    } else {
      iconAudioOn.style.display = 'block';
      iconAudioOff.style.display = 'none';
      audio.playNav();
    }
  });

  const volSlider = document.getElementById('volume-slider');
  if (volSlider) {
    volSlider.addEventListener('input', (e) => {
      audio.volume = e.target.value / 100;
      audio.playNav();
    });
  }

  // =========================================================================
  // 10. KEYBOARD & CONTROLLER NAVIGATION
  // Full D-Pad / Arrow Key, Gamepad API, and Shortcut integration
  // =========================================================================
  window.addEventListener('keydown', (e) => {
    if (e.target.tagName === 'INPUT') return;

    if (e.key === 'ArrowRight') {
      e.preventDefault();
      if (state.activeModal) return;
      if (state.activeDockIndex >= 0) {
        state.activeDockIndex = (state.activeDockIndex + 1) % dockButtons.length;
        dockButtons.forEach((b, i) => b.classList.toggle('active', i === state.activeDockIndex));
        audio.playNav();
      } else {
        selectGame(Math.min(state.currentIndex + 1, state.filteredGames.length - 1));
      }
    } else if (e.key === 'ArrowLeft') {
      e.preventDefault();
      if (state.activeModal) return;
      if (state.activeDockIndex >= 0) {
        state.activeDockIndex = (state.activeDockIndex - 1 + dockButtons.length) % dockButtons.length;
        dockButtons.forEach((b, i) => b.classList.toggle('active', i === state.activeDockIndex));
        audio.playNav();
      } else {
        selectGame(Math.max(state.currentIndex - 1, 0));
      }
    } else if (e.key === 'ArrowDown') {
      e.preventDefault();
      if (state.activeModal) return;
      if (state.activeDockIndex < 0) {
        state.activeDockIndex = 0;
        dockButtons.forEach((b, i) => b.classList.toggle('active', i === 0));
        audio.playNav();
        updateLegendContext();
      }
    } else if (e.key === 'ArrowUp') {
      e.preventDefault();
      if (state.activeModal) return;
      if (state.activeDockIndex >= 0) {
        state.activeDockIndex = -1;
        dockButtons.forEach(b => b.classList.remove('active'));
        document.getElementById('dock-home').classList.add('active');
        audio.playNav();
        updateLegendContext();
      }
    } else if (e.key === 'Enter' || e.key === ' ') {
      e.preventDefault();
      // Button Ⓐ Confirm
      if (state.activeModal === 'game') {
        document.getElementById('btn-launch-game').click();
      } else if (state.activeModal) {
        closeModal();
      } else if (state.activeDockIndex >= 0) {
        dockButtons[state.activeDockIndex].click();
      } else if (state.activeView === 'dashboard') {
        audio.playSelect();
        openGameModal(state.filteredGames[state.currentIndex]);
      } else if (state.activeView === 'spec') {
        document.getElementById('toggle-focus-mode').click();
      }
    } else if (e.key === 'Escape' || e.key === 'Backspace') {
      e.preventDefault();
      // Button Ⓑ Back
      if (state.activeModal) {
        closeModal();
      } else if (state.activeView === 'spec') {
        switchView('dashboard');
      } else if (state.activeDockIndex >= 0) {
        state.activeDockIndex = -1;
        dockButtons.forEach(b => b.classList.remove('active'));
        document.getElementById('dock-home').classList.add('active');
        audio.playBack();
        updateLegendContext();
      }
    } else if (e.key === 'x' || e.key === 'X') {
      e.preventDefault();
      // Button Ⓧ Options / Details
      if (state.activeModal === 'game') {
        document.getElementById('btn-save-state').click();
      } else if (!state.activeModal) {
        audio.playSelect();
        openGameModal(state.filteredGames[state.currentIndex]);
      }
    } else if (e.key === 'y' || e.key === 'Y') {
      e.preventDefault();
      // Button Ⓨ Mode Switch
      const nextView = state.activeView === 'dashboard' ? 'spec' : 'dashboard';
      switchView(nextView);
    } else if (e.key === 'm' || e.key === 'M') {
      btnToggleAudio.click();
    }
  });

  // Gamepad Loop (Polling navigator.getGamepads())
  let lastGamepadButtonState = {};
  let lastStickNavTime = 0;

  function pollGamepad() {
    const gamepads = navigator.getGamepads ? navigator.getGamepads() : [];
    let connected = false;

    for (let i = 0; i < gamepads.length; i++) {
      const gp = gamepads[i];
      if (gp && gp.connected) {
        connected = true;
        handleGamepadInput(gp);
        break;
      }
    }

    const gpIndicator = document.getElementById('gamepad-status');
    if (gpIndicator) {
      gpIndicator.style.opacity = connected ? '1' : '0.35';
    }

    requestAnimationFrame(pollGamepad);
  }

  function handleGamepadInput(gp) {
    const now = Date.now();

    // D-Pad or Left Stick
    const stickX = gp.axes[0] || 0;
    const stickY = gp.axes[1] || 0;
    const dpadLeft = gp.buttons[14] && gp.buttons[14].pressed;
    const dpadRight = gp.buttons[15] && gp.buttons[15].pressed;
    const dpadUp = gp.buttons[12] && gp.buttons[12].pressed;
    const dpadDown = gp.buttons[13] && gp.buttons[13].pressed;

    if (now - lastStickNavTime > 220) {
      if (stickX > 0.5 || dpadRight) {
        lastStickNavTime = now;
        window.dispatchEvent(new KeyboardEvent('keydown', { key: 'ArrowRight' }));
      } else if (stickX < -0.5 || dpadLeft) {
        lastStickNavTime = now;
        window.dispatchEvent(new KeyboardEvent('keydown', { key: 'ArrowLeft' }));
      } else if (stickY > 0.5 || dpadDown) {
        lastStickNavTime = now;
        window.dispatchEvent(new KeyboardEvent('keydown', { key: 'ArrowDown' }));
      } else if (stickY < -0.5 || dpadUp) {
        lastStickNavTime = now;
        window.dispatchEvent(new KeyboardEvent('keydown', { key: 'ArrowUp' }));
      }
    }

    // Buttons
    // Button 0 = A (Nintendo) / Cross (PS)
    if (gp.buttons[0] && gp.buttons[0].pressed && !lastGamepadButtonState[0]) {
      window.dispatchEvent(new KeyboardEvent('keydown', { key: 'Enter' }));
    }
    // Button 1 = B (Nintendo) / Circle (PS)
    if (gp.buttons[1] && gp.buttons[1].pressed && !lastGamepadButtonState[1]) {
      window.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape' }));
    }
    // Button 2 = X (Nintendo) / Square (PS)
    if (gp.buttons[2] && gp.buttons[2].pressed && !lastGamepadButtonState[2]) {
      window.dispatchEvent(new KeyboardEvent('keydown', { key: 'x' }));
    }
    // Button 3 = Y (Nintendo) / Triangle (PS)
    if (gp.buttons[3] && gp.buttons[3].pressed && !lastGamepadButtonState[3]) {
      window.dispatchEvent(new KeyboardEvent('keydown', { key: 'y' }));
    }

    gp.buttons.forEach((b, idx) => {
      lastGamepadButtonState[idx] = b.pressed;
    });
  }

  // =========================================================================
  // 11. QUIET STATUS CLOCK (Rule 7)
  // Low-contrast live clock updating every second
  // =========================================================================
  function updateClock() {
    const clockEl = document.getElementById('live-clock');
    if (!clockEl) return;
    const now = new Date();
    let hours = now.getHours();
    const minutes = String(now.getMinutes()).padStart(2, '0');
    const ampm = hours >= 12 ? 'PM' : 'AM';
    hours = hours % 12;
    hours = hours ? hours : 12; // 0 becomes 12
    clockEl.textContent = `${hours}:${minutes} ${ampm}`;
  }

  setInterval(updateClock, 1000);
  updateClock();

  // Try reading battery from system if available
  if (navigator.getBattery) {
    navigator.getBattery().then(batt => {
      const fill = document.getElementById('battery-level-fill');
      const label = document.getElementById('battery-pct-label');
      const bolt = document.getElementById('battery-charging-bolt');

      function updateBatt() {
        const pct = Math.round(batt.level * 100);
        if (fill) fill.style.width = `${pct}%`;
        if (label) label.textContent = `${pct}%`;
        if (bolt) bolt.style.display = batt.charging ? 'block' : 'none';
        if (pct <= 15) {
          fill.classList.add('low-charge');
        } else {
          fill.classList.remove('low-charge');
        }
      }
      updateBatt();
      batt.addEventListener('levelchange', updateBatt);
      batt.addEventListener('chargingchange', updateBatt);
    }).catch(() => {});
  }

  // =========================================================================
  // 12. INITIALIZATION
  // =========================================================================
  renderShelf();
  requestAnimationFrame(pollGamepad);

  // Resize handler for shelf re-centering
  window.addEventListener('resize', () => {
    updateShelfPosition();
  });

})();
