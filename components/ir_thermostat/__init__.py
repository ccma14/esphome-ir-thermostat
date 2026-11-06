# Author: Christian Czezatke (@ccma14)
from pathlib import Path
import esphome.config_validation as cv
from esphome.core import CORE, CoroPriority, coroutine_with_priority
from esphome.core.config import include_file

CODEOWNERS = ["@ccma14"]
CONFIG_SCHEMA = cv.Schema({})


@coroutine_with_priority(CoroPriority.FINAL)
async def _add_ir_thermostat_header() -> None:
    header = Path(__file__).resolve().parent.parent.parent / "common" / "ir_thermostat.h"
    include_file(header, Path("ir_thermostat.h"), False)


async def to_code(config: dict) -> None:
    CORE.add_job(_add_ir_thermostat_header)

