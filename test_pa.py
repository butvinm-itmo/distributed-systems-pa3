import re
import subprocess
from dataclasses import dataclass
from pathlib import Path

import pytest


def run_program(*args: str, timeout: int = 30) -> tuple[int, str, str]:
    cmd = ["./run.sh"] + list(args)
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    return result.returncode, result.stdout, result.stderr


def build_with_source(source_code: str) -> None:
    Path("bank_robbery.c").write_text(source_code)
    subprocess.run(["./build.sh"], check=True, capture_output=True)


@dataclass
class Transfer:
    src: int
    dst: int
    amount: int


@dataclass
class TransferTestCase:
    test_id: str
    description: str
    num_processes: int
    initial_balances: list[int]
    robbery_source_code: str
    expected_transfers: list[Transfer]
    expected_final_balances: list[int]
    expected_total_balance: int


@pytest.mark.parametrize(
    argnames="test_case",
    argvalues=[
        TransferTestCase(
            test_id="forward_circle_3proc",
            description="Forward Circle: 1→2 $1, 2→3 $2, 3→1 $1",
            num_processes=3,
            initial_balances=[10, 20, 30],
            robbery_source_code="""
            #include "banking.h"

            void bank_robbery(void * parent_data, local_id max_id)
            {
                for (int i = 1; i < max_id; ++i) {
                    transfer(parent_data, i, i + 1, i);
                }
                if (max_id > 1) {
                    transfer(parent_data, max_id, 1, 1);
                }
            }
            """,
            expected_transfers=[
                Transfer(src=1, dst=2, amount=1),
                Transfer(src=2, dst=3, amount=2),
                Transfer(src=3, dst=1, amount=1),
            ],
            expected_final_balances=[10, 19, 31],
            expected_total_balance=60,
        ),
        TransferTestCase(
            test_id="forward_circle_4proc",
            description="Forward Circle: 1→2 $1, 2→3 $2, 3→4 $3, 4→1 $1",
            num_processes=4,
            initial_balances=[5, 10, 15, 20],
            robbery_source_code="""
            #include "banking.h"

            void bank_robbery(void * parent_data, local_id max_id)
            {
                for (int i = 1; i < max_id; ++i) {
                    transfer(parent_data, i, i + 1, i);
                }
                if (max_id > 1) {
                    transfer(parent_data, max_id, 1, 1);
                }
            }
            """,
            expected_transfers=[
                Transfer(src=1, dst=2, amount=1),
                Transfer(src=2, dst=3, amount=2),
                Transfer(src=3, dst=4, amount=3),
                Transfer(src=4, dst=1, amount=1),
            ],
            expected_final_balances=[5, 9, 14, 22],
            expected_total_balance=50,
        ),
        TransferTestCase(
            test_id="star_pattern",
            description="Star Pattern - All to One: 2→1 $2, 3→1 $3",
            num_processes=3,
            initial_balances=[10, 20, 30],
            robbery_source_code="""
            #include "banking.h"

            void bank_robbery(void * parent_data, local_id max_id)
            {
                for (int i = 2; i <= max_id; ++i) {
                    transfer(parent_data, i, 1, i);
                }
            }
            """,
            expected_transfers=[
                Transfer(src=2, dst=1, amount=2),
                Transfer(src=3, dst=1, amount=3),
            ],
            expected_final_balances=[15, 18, 27],
            expected_total_balance=60,
        ),
        TransferTestCase(
            test_id="robin_hood",
            description="Robin Hood - Redistribute Wealth: 3→1 $1, 2→1 $2",
            num_processes=3,
            initial_balances=[10, 20, 30],
            robbery_source_code="""
            #include "banking.h"

            void bank_robbery(void * parent_data, local_id max_id)
            {
                if (max_id >= 2) {
                    for (int i = max_id; i >= 2; --i) {
                        int amount = max_id - i + 1;
                        transfer(parent_data, i, 1, amount);
                    }
                }
            }
            """,
            expected_transfers=[
                Transfer(src=3, dst=1, amount=1),
                Transfer(src=2, dst=1, amount=2),
            ],
            expected_final_balances=[13, 18, 29],
            expected_total_balance=60,
        ),
        TransferTestCase(
            test_id="backward_circle",
            description="Chain Reversal - Backward Circle: 3→2 $3, 2→1 $2, 1→3 $3",
            num_processes=3,
            initial_balances=[10, 20, 30],
            robbery_source_code="""
            #include "banking.h"

            void bank_robbery(void * parent_data, local_id max_id)
            {
                for (int i = max_id; i >= 2; --i) {
                    transfer(parent_data, i, i - 1, i);
                }
                if (max_id > 1) {
                    transfer(parent_data, 1, max_id, max_id);
                }
            }
            """,
            expected_transfers=[
                Transfer(src=3, dst=2, amount=3),
                Transfer(src=2, dst=1, amount=2),
                Transfer(src=1, dst=3, amount=3),
            ],
            expected_final_balances=[9, 21, 30],
            expected_total_balance=60,
        ),
        TransferTestCase(
            test_id="forward_circle_10proc",
            description="Forward Circle with 10 processes",
            num_processes=10,
            initial_balances=[10, 20, 30, 40, 50, 60, 70, 80, 90, 100],
            robbery_source_code="""
            #include "banking.h"

            void bank_robbery(void * parent_data, local_id max_id)
            {
                for (int i = 1; i < max_id; ++i) {
                    transfer(parent_data, i, i + 1, i);
                }
                if (max_id > 1) {
                    transfer(parent_data, max_id, 1, 1);
                }
            }
            """,
            expected_transfers=[
                Transfer(src=1, dst=2, amount=1),
                Transfer(src=2, dst=3, amount=2),
                Transfer(src=3, dst=4, amount=3),
                Transfer(src=4, dst=5, amount=4),
                Transfer(src=5, dst=6, amount=5),
                Transfer(src=6, dst=7, amount=6),
                Transfer(src=7, dst=8, amount=7),
                Transfer(src=8, dst=9, amount=8),
                Transfer(src=9, dst=10, amount=9),
                Transfer(src=10, dst=1, amount=1),
            ],
            expected_final_balances=[10, 19, 29, 39, 49, 59, 69, 79, 89, 108],
            expected_total_balance=550,
        ),
    ],
    ids=lambda test_case: test_case.test_id,
)
def test_transfer(test_case: TransferTestCase) -> None:
    build_with_source(test_case.robbery_source_code)

    ret, stdout, stderr = run_program(
        "-p",
        str(test_case.num_processes),
        *[str(b) for b in test_case.initial_balances],
    )

    assert ret == 0

    events_log = Path("events.log")
    assert events_log.exists()

    events = events_log.read_text()
    total_processes = test_case.num_processes + 1

    for i in range(total_processes):
        assert re.search(
            rf"^[0-9]+: process {i} received all STARTED messages$",
            events,
            re.MULTILINE,
        )
        assert re.search(
            rf"^[0-9]+: process {i} received all DONE messages$",
            events,
            re.MULTILINE,
        )

    for i in range(1, test_case.num_processes + 1):
        initial_balance = test_case.initial_balances[i - 1]
        final_balance = test_case.expected_final_balances[i - 1]
        assert re.search(
            rf"^[0-9]+: process {i} \(pid [0-9]+, parent [0-9]+\) has STARTED with balance \$\s*{initial_balance}$",
            events,
            re.MULTILINE,
        )
        assert re.search(
            rf"^[0-9]+: process {i} has DONE with balance \$\s*{final_balance}$",
            events,
            re.MULTILINE,
        )

    for transfer in test_case.expected_transfers:
        assert re.search(
            rf"process {transfer.src} transferred \$ {transfer.amount} to process {transfer.dst}",
            events,
        )

    assert "Full balance history" in stdout
    for i in range(1, test_case.num_processes + 1):
        assert re.search(rf"^\s*{i}\s*\|", stdout, re.MULTILINE)

    assert re.search(r"^\s*Total\s*\|", stdout, re.MULTILINE)

    assert re.search(rf"Total.*{test_case.expected_total_balance}", stdout)

    total_line_match = re.search(r"^\s*Total\s*\|(.+)$", stdout, re.MULTILINE)
    assert total_line_match

    total_line = total_line_match.group(1)
    total_values = re.findall(r"\s*(\d+)\s*(?:\||$)", total_line)

    for i, total_str in enumerate(total_values):
        total_at_time = int(total_str)
        assert total_at_time == test_case.expected_total_balance
