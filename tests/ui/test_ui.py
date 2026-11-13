"""Selenium system tests for the web frontend (STP Appendix A).

  TC-UI-01  select a folder in the browser and press Compress
  TC-UI-02  select an archive and extract it; files identical to the originals
  TC-UI-03  progress screen, then a results screen with size, ratio, time,
            speed and the transform breakdown
  TC-UI-04  forced failures show a plain-language message, never a raw code
            or a stack trace
  TC-UI-05  the page stays responsive and keeps polling during a long job
  TC-INFO-01 / TC-VER-01 through the UI
  NFR-04    keyboard use and focus handling (SAD 4.5 accessibility)

Run:  make cli && python3 -m pytest tests/ui -v
"""
import hashlib
import re
import shutil
import time

import pytest
from selenium.webdriver.common.by import By
from selenium.webdriver.common.keys import Keys

# SAD 4.3.4 plain-language messages the UI must show.
MSG_MALFORMED = "This file is not a valid UATC archive or is damaged."
MSG_PERMISSION = "UATC is not allowed to open or write that location."
MSG_NOT_FOUND = "We could not find that file or folder."
MSG_CORRUPT = {
    "This archive is corrupted and cannot be extracted safely.",
    "This archive failed its integrity check.",
}

RAW_CODE = re.compile(r"\bE_[A-Z_]+\b")


def sha256_tree(folder):
    """{relative path: sha256} for every file under folder."""
    return {
        str(p.relative_to(folder)): hashlib.sha256(p.read_bytes()).hexdigest()
        for p in sorted(folder.rglob("*")) if p.is_file()
    }


def compress_folder_via_browser(ui, folder_name):
    """Home -> Compress -> Browse -> open folder -> Use this folder -> Start."""
    ui.open_live()
    ui.click("btn-compress")
    ui.click("btn-browse-source")
    ui.browse_open_folder(folder_name)
    ui.click("browser-select")
    ui.click("btn-start")
    return ui.wait_screen("result", "error")


# ------------------------------------------------------------------ TC-UI-01

def test_tc_ui_01_compress_a_folder(ui, corpus):
    archive = ui.root / "project.uatc"
    archive.unlink(missing_ok=True)

    ui.open_live()
    ui.click("btn-compress")
    assert ui.text("select-title") == "Compress"

    ui.click("btn-browse-source")
    ui.browse_open_folder("project")
    ui.click("browser-select")

    # The source is the chosen folder and the destination is suggested.
    assert ui.el("source").get_attribute("value") == str(corpus)
    assert ui.el("destination").get_attribute("value") == str(archive)

    ui.click("btn-start")
    assert ui.wait_screen("result", "error") == "result", ui.text("error-text")

    assert archive.exists() and archive.stat().st_size > 0
    body = ui.text("result-body")
    assert "The archive was created." in body
    assert re.search(r"Files\s+4", body), body


# ------------------------------------------------------------------ TC-UI-02

def test_tc_ui_02_extract_restores_identical_files(ui, corpus):
    assert compress_folder_via_browser(ui, "project") == "result"
    restored = ui.root / "restored_ui02"
    shutil.rmtree(restored, ignore_errors=True)

    ui.click("btn-again")
    ui.click("btn-extract")
    ui.click("btn-browse-source")
    ui.browse_choose_file("project.uatc")
    ui.type("destination", str(restored))
    ui.click("btn-start")
    assert ui.wait_screen("result", "error") == "result", ui.text("error-text")

    assert "restored, byte for byte" in ui.text("result-body")
    assert sha256_tree(restored) == sha256_tree(corpus)


# ------------------------------------------------------------------ TC-UI-03

def test_tc_ui_03_progress_then_full_results(ui, corpus):
    ui.open_live()
    ui.click("btn-compress")
    ui.type("source", str(corpus / "data.csv"))
    ui.type("destination", str(ui.root / "data.uatc"))
    ui.click("btn-start")

    # The progress screen appears with status, bar, percentage and clock.
    ui.wait_visible("screen-progress", timeout=5)
    assert ui.text("progress-status") in {
        "Waiting to start…", "Compressing…"}
    assert re.fullmatch(r"\d{1,3}%", ui.text("progress-text"))
    assert re.fullmatch(r"Elapsed: \d\d:\d\d", ui.text("progress-elapsed"))

    assert ui.wait_screen("result", "error") == "result", ui.text("error-text")
    body = ui.text("result-body")
    for label in ("Original size", "Archive size", "Compression ratio",
                  "Time", "Speed", "Blocks stored as-is"):
        assert label in body, f"missing {label!r} in results"
    assert "Method used for each part" in body
    assert RAW_CODE.search(body) is None


# ------------------------------------------------------------------ TC-UI-04

def test_tc_ui_04_bad_archive_shows_plain_message(ui, corpus):
    ui.open_live()
    ui.click("btn-info")
    ui.type("source", str(corpus / "README.md"))   # exists, not an archive
    ui.click("btn-start")
    assert ui.wait_screen("result", "error") == "error"
    message = ui.text("error-text")
    assert message == MSG_MALFORMED
    assert RAW_CODE.search(ui.text("screen-error")) is None
    assert "Traceback" not in message and "exception" not in message.lower()


def test_tc_ui_04_corrupted_archive_on_verify(ui, corpus):
    assert compress_folder_via_browser(ui, "project") == "result"
    damaged = ui.root / "damaged.uatc"
    data = bytearray((ui.root / "project.uatc").read_bytes())
    data[len(data) // 2] ^= 0x5A                  # flip one payload byte
    damaged.write_bytes(bytes(data))

    ui.click("btn-again")
    ui.click("btn-verify")
    ui.type("source", str(damaged))
    ui.click("btn-start")
    assert ui.wait_screen("result", "error") == "error"
    assert ui.text("error-text") in MSG_CORRUPT | {MSG_MALFORMED}
    assert RAW_CODE.search(ui.text("screen-error")) is None


def test_tc_ui_04_path_outside_allowed_folder(ui, corpus):
    ui.open_live()
    ui.click("btn-info")
    ui.type("source", "/etc/passwd")
    ui.click("btn-start")
    assert ui.wait_screen("result", "error") == "error"
    assert ui.text("error-text") == MSG_PERMISSION


def test_tc_ui_04_unwritable_destination(ui, corpus):
    ui.open_live()
    ui.click("btn-compress")
    ui.type("source", str(corpus / "README.md"))
    # The parent is a file, so the destination can never be written.
    ui.type("destination", str(corpus / "README.md" / "out.uatc"))
    ui.click("btn-start")
    assert ui.wait_screen("result", "error") == "error"
    message = ui.text("error-text")
    assert RAW_CODE.search(message) is None
    assert "disk could not be read or written" in message or message == MSG_PERMISSION


def test_tc_ui_04_missing_input_is_caught_in_the_form(ui, corpus):
    ui.open_live()
    ui.click("btn-compress")
    ui.click("btn-start")                           # nothing filled in
    assert ui.visible("form-error")
    assert ui.el("source").get_attribute("aria-invalid") == "true"
    assert ui.d.switch_to.active_element.get_attribute("id") == "source"
    assert ui.visible("screen-select")              # no jump to the error page


def test_tc_ui_04_try_again_keeps_the_input(ui, corpus):
    ui.open_live()
    ui.click("btn-info")
    ui.type("source", "missing.uatc")
    ui.click("btn-start")
    assert ui.wait_screen("result", "error") == "error"
    assert ui.text("error-text") == MSG_NOT_FOUND
    ui.click("btn-error-retry")
    assert ui.visible("screen-select")
    assert ui.el("source").get_attribute("value") == "missing.uatc"


# ------------------------------------------------------------------ TC-UI-05

@pytest.fixture
def big_file(server):
    """~80 MB of CSV-like text: several seconds of work for the backend."""
    _, _, root = server
    path = root / "big.csv"
    if not path.exists():
        with path.open("w") as out:
            for chunk in range(80):
                out.write("".join(
                    f"{chunk},{i},sensor-{i % 97},{(i * 37) % 1000}.{i % 10},ok\n"
                    for i in range(25000)))
    return path


def test_tc_ui_05_page_stays_responsive_during_a_long_job(ui, big_file):
    ui.open_live()
    ui.click("btn-compress")
    ui.type("source", str(big_file))
    ui.type("destination", str(ui.root / "big.uatc"))
    ui.click("btn-start")
    ui.wait_visible("screen-progress", timeout=5)

    seen_progress = []
    worst_round_trip = 0.0
    while ui.visible("screen-progress"):
        # The main thread answers immediately: no long task is blocking it.
        start = time.monotonic()
        value = ui.d.execute_script(
            "return document.getElementById('progress-bar').value")
        worst_round_trip = max(worst_round_trip, time.monotonic() - start)
        seen_progress.append(value)
        # The user can still interact: open and close Advanced-free UI bits.
        ui.d.execute_script("document.getElementById('progress-title').focus()")
        time.sleep(0.25)
        if len(seen_progress) > 400:
            pytest.fail("job did not finish in time")

    assert worst_round_trip < 1.0, f"page blocked for {worst_round_trip:.2f}s"
    # Polling continued: the bar moved forward while the job ran.
    assert len(set(seen_progress)) >= 2, seen_progress
    assert seen_progress == sorted(seen_progress), "progress went backwards"
    assert ui.wait_screen("result", "error") == "result", ui.text("error-text")


def test_tc_ui_05_leaving_progress_keeps_the_job_running(ui, big_file):
    target = ui.root / "big_leave.uatc"
    target.unlink(missing_ok=True)

    ui.open_live()
    ui.click("btn-compress")
    ui.type("source", str(big_file))
    ui.type("destination", str(target))
    ui.click("btn-start")
    ui.wait_visible("screen-progress", timeout=5)
    ui.click("btn-progress-home")
    assert ui.visible("screen-home")

    # The page is fully usable while the backend keeps working.
    ui.click("btn-info")
    assert ui.text("select-title") == "Archive Info"

    deadline = time.time() + 120
    while not target.exists() and time.time() < deadline:
        time.sleep(0.5)
    assert target.exists(), "the job did not keep running after leaving"


# ------------------------------------------------- TC-INFO-01 / TC-VER-01

def test_tc_info_01_info_through_the_ui(ui, corpus):
    assert compress_folder_via_browser(ui, "project") == "result"
    ui.click("btn-again")
    ui.click("btn-info")
    ui.click("btn-browse-source")
    ui.browse_choose_file("project.uatc")
    ui.click("btn-start")
    assert ui.wait_screen("result", "error") == "result"
    assert ui.text("result-title") == "Archive information"
    body = ui.text("result-body")
    assert re.search(r"Files\s+4", body)
    for name in ("README.md", "data.csv", "src/main.cpp", "empty.txt"):
        assert name in body, f"{name} missing from the file list"
    assert "Nothing was extracted." in body


def test_tc_ver_01_verify_through_the_ui(ui, corpus):
    assert compress_folder_via_browser(ui, "project") == "result"
    ui.click("btn-again")
    ui.click("btn-verify")
    ui.type("source", str(ui.root / "project.uatc"))
    ui.click("btn-start")
    assert ui.wait_screen("result", "error") == "result"
    assert ui.text("result-title") == "Archive is OK"
    assert "Nothing was written." in ui.text("result-body")


# ---------------------------------------------- accessibility (SAD 4.5)

def test_keyboard_only_compress_flow(ui, corpus):
    ui.open_live()
    body = ui.d.find_element(By.TAG_NAME, "body")
    body.send_keys(Keys.TAB)        # skip link
    body = ui.d.switch_to.active_element
    body.send_keys(Keys.TAB)        # first operation
    assert ui.d.switch_to.active_element.get_attribute("id") == "btn-compress"
    ui.d.switch_to.active_element.send_keys(Keys.ENTER)

    # Focus moves to the new screen's heading.
    assert ui.d.switch_to.active_element.get_attribute("id") == "select-title"
    ui.el("source").send_keys(str(corpus / "README.md"))
    ui.el("destination").send_keys(str(ui.root / "readme.uatc"), Keys.ENTER)
    assert ui.wait_screen("result", "error") == "result"
    assert ui.d.switch_to.active_element.get_attribute("id") == "result-title"


def test_every_input_has_a_label(ui):
    """Every input has an accessible name: a <label for=...>, or an
    aria-label (used by the hidden file input behind the Upload... button)."""
    ui.open_live()
    for field in ui.d.find_elements(By.CSS_SELECTOR, "input"):
        field_id = field.get_attribute("id")
        labels = ui.d.find_elements(By.CSS_SELECTOR, f'label[for="{field_id}"]')
        aria = (field.get_attribute("aria-label") or "").strip()
        assert labels or aria, f"input #{field_id} has no <label> and no aria-label"


# ----------------------------------------------------- demo mode (no backend)

def test_demo_mode_banner_and_results(ui):
    ui.open_demo()
    assert "Demo mode" in ui.text("mode-banner")
    ui.click("btn-compress")
    ui.click("btn-browse-source")
    ui.browse_open_folder("project")
    ui.click("browser-select")
    ui.click("btn-start")
    assert ui.wait_screen("result", "error") == "result"
    assert "Demo only" in ui.text("result-body")


def test_demo_mode_failure_path(ui):
    ui.open_demo(fail=True)
    ui.click("btn-verify")
    ui.type("source", "/home/demo/project.uatc")
    ui.click("btn-start")
    assert ui.wait_screen("result", "error") == "error"
    assert ui.text("error-text") in MSG_CORRUPT
