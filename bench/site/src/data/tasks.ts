import type { AccuracyTask, TaskCategory } from "../types/benchmark";

/**
 * Layer 1: Infrastructure speed benchmark task definitions.
 * These are deterministic — no LLM involved.
 */
export interface SpeedTask {
  id: string;
  name: string;
  description: string;
  category: TaskCategory;
  iterations: number;
}

export const SPEED_TASKS: SpeedTask[] = [
  {
    id: "cold_start",
    name: "Cold Start + Navigate",
    description: "Create browser session and navigate to about:blank",
    category: "startup",
    iterations: 10,
  },
  {
    id: "navigate_complex",
    name: "Navigate (complex page)",
    description: "Navigate to Wikipedia article, wait for load complete",
    category: "navigation",
    iterations: 10,
  },
  {
    id: "snapshot_full",
    name: "Full DOM Snapshot",
    description: "Capture complete accessibility tree of Wikipedia article",
    category: "snapshot",
    iterations: 10,
  },
  {
    id: "snapshot_interactive",
    name: "Interactive-Only Snapshot",
    description: "Capture snapshot filtered to buttons, links, inputs only",
    category: "snapshot",
    iterations: 10,
  },
  {
    id: "screenshot_plain",
    name: "Screenshot (PNG)",
    description: "Full-page screenshot encoded as PNG",
    category: "screenshot",
    iterations: 10,
  },
  {
    id: "screenshot_annotated",
    name: "Annotated Screenshot",
    description: "Screenshot with numbered [N] labels on interactive elements",
    category: "screenshot",
    iterations: 10,
  },
  {
    id: "eval_simple",
    name: "JS Eval (simple)",
    description: "Evaluate document.title and return string result",
    category: "eval",
    iterations: 10,
  },
  {
    id: "eval_complex",
    name: "JS Eval (100 elements)",
    description:
      "Extract tag, text, href, bounding rect from first 100 interactive elements",
    category: "eval",
    iterations: 10,
  },
  {
    id: "click_wait_snapshot",
    name: "Click + Wait + Snapshot",
    description: "Click first link, wait for navigation, capture snapshot",
    category: "compound",
    iterations: 10,
  },
  {
    id: "navigate_10x",
    name: "10x Sequential Navigations",
    description: "Navigate to 10 different pages back-to-back",
    category: "throughput",
    iterations: 3,
  },
  {
    id: "state_save_load",
    name: "State Save + Load",
    description: "Save cookies/storage to disk, then restore",
    category: "state",
    iterations: 10,
  },
];

/**
 * Layer 2: Online-Mind2Web subset for accuracy comparison.
 * 30 tasks (10 easy, 15 medium, 5 hard) from the public task set.
 */
export const ACCURACY_TASKS: AccuracyTask[] = [
  // === Easy (10 tasks) ===
  {
    taskId: "m2w-easy-001",
    instruction: "Open the reviews of a recipe with beef sirloin",
    website: "allrecipes.com",
    difficulty: "easy",
    domain: "food",
  },
  {
    taskId: "m2w-easy-002",
    instruction: "Find the contact phone number for customer support",
    website: "amazon.com",
    difficulty: "easy",
    domain: "shopping",
  },
  {
    taskId: "m2w-easy-003",
    instruction: "Search for 'machine learning' and open the first result",
    website: "google.com",
    difficulty: "easy",
    domain: "search",
  },
  {
    taskId: "m2w-easy-004",
    instruction: "Find the current weather for San Francisco",
    website: "weather.com",
    difficulty: "easy",
    domain: "weather",
  },
  {
    taskId: "m2w-easy-005",
    instruction: "Navigate to the 'About' page",
    website: "wikipedia.org",
    difficulty: "easy",
    domain: "reference",
  },
  {
    taskId: "m2w-easy-006",
    instruction: "Open the top trending repository",
    website: "github.com",
    difficulty: "easy",
    domain: "development",
  },
  {
    taskId: "m2w-easy-007",
    instruction: "Find the store hours for the nearest location",
    website: "target.com",
    difficulty: "easy",
    domain: "shopping",
  },
  {
    taskId: "m2w-easy-008",
    instruction: "Check the price of Bitcoin",
    website: "coinmarketcap.com",
    difficulty: "easy",
    domain: "finance",
  },
  {
    taskId: "m2w-easy-009",
    instruction: "Open the sports section",
    website: "cnn.com",
    difficulty: "easy",
    domain: "news",
  },
  {
    taskId: "m2w-easy-010",
    instruction: "Find the latest episode of a top podcast",
    website: "spotify.com",
    difficulty: "easy",
    domain: "media",
  },

  // === Medium (15 tasks) ===
  {
    taskId: "m2w-med-001",
    instruction:
      "Find full-time legal jobs in San Diego County, minimum $4,000+/month",
    website: "ca.gov",
    difficulty: "medium",
    domain: "government",
  },
  {
    taskId: "m2w-med-002",
    instruction:
      "Add a pair of Nike running shoes size 10 to the cart and proceed to checkout",
    website: "nike.com",
    difficulty: "medium",
    domain: "shopping",
  },
  {
    taskId: "m2w-med-003",
    instruction:
      "Find round-trip flights from JFK to LAX for next weekend, sort by price",
    website: "google.com/travel/flights",
    difficulty: "medium",
    domain: "travel",
  },
  {
    taskId: "m2w-med-004",
    instruction:
      "Search for 3-bedroom apartments in Austin, TX under $2000/month with pets allowed",
    website: "zillow.com",
    difficulty: "medium",
    domain: "housing",
  },
  {
    taskId: "m2w-med-005",
    instruction:
      "Find a recipe for chocolate lava cake with less than 30 minutes prep time",
    website: "allrecipes.com",
    difficulty: "medium",
    domain: "food",
  },
  {
    taskId: "m2w-med-006",
    instruction:
      "Compare the specifications of iPhone 16 Pro and Samsung Galaxy S25",
    website: "gsmarena.com",
    difficulty: "medium",
    domain: "tech",
  },
  {
    taskId: "m2w-med-007",
    instruction:
      "Find the top-rated Italian restaurant in Manhattan with outdoor seating",
    website: "yelp.com",
    difficulty: "medium",
    domain: "dining",
  },
  {
    taskId: "m2w-med-008",
    instruction:
      "Create a new public repository with a README and MIT license",
    website: "github.com",
    difficulty: "medium",
    domain: "development",
  },
  {
    taskId: "m2w-med-009",
    instruction: "Find scholarships for computer science students in California",
    website: "scholarships.com",
    difficulty: "medium",
    domain: "education",
  },
  {
    taskId: "m2w-med-010",
    instruction:
      "Book a table for 4 people at a sushi restaurant this Saturday at 7pm",
    website: "opentable.com",
    difficulty: "medium",
    domain: "dining",
  },
  {
    taskId: "m2w-med-011",
    instruction:
      "Find the cheapest auto insurance quote for a 2022 Honda Civic",
    website: "progressive.com",
    difficulty: "medium",
    domain: "finance",
  },
  {
    taskId: "m2w-med-012",
    instruction:
      "Search for remote software engineer jobs paying over $150k at companies with 4+ star ratings",
    website: "glassdoor.com",
    difficulty: "medium",
    domain: "jobs",
  },
  {
    taskId: "m2w-med-013",
    instruction:
      "Find a hotel in Paris with free cancellation, breakfast included, under $200/night for next month",
    website: "booking.com",
    difficulty: "medium",
    domain: "travel",
  },
  {
    taskId: "m2w-med-014",
    instruction:
      "Look up the side effects and interactions of ibuprofen with aspirin",
    website: "webmd.com",
    difficulty: "medium",
    domain: "health",
  },
  {
    taskId: "m2w-med-015",
    instruction:
      "Find the best-selling science fiction book this year and check if the library has it",
    website: "goodreads.com",
    difficulty: "medium",
    domain: "media",
  },

  // === Hard (5 tasks) ===
  {
    taskId: "m2w-hard-001",
    instruction:
      "Find the cheapest hotel + flight + car package from New York to " +
      "San Francisco, for two adults and a six-year-old child, " +
      "with free breakfast and spa access",
    website: "booking.com",
    difficulty: "hard",
    domain: "travel",
  },
  {
    taskId: "m2w-hard-002",
    instruction:
      "Compare 3 different health insurance plans for a family of 4 in " +
      "California, extract monthly premiums, deductibles, and out-of-pocket maximums",
    website: "healthcare.gov",
    difficulty: "hard",
    domain: "finance",
  },
  {
    taskId: "m2w-hard-003",
    instruction:
      "Find a used Tesla Model 3 within 50 miles, under $30k, less than " +
      "30k miles, and schedule a test drive",
    website: "carvana.com",
    difficulty: "hard",
    domain: "shopping",
  },
  {
    taskId: "m2w-hard-004",
    instruction:
      "Plan a 3-day itinerary in Tokyo including flights from SFO, hotel " +
      "near Shibuya, restaurant reservations, and museum tickets",
    website: "google.com/travel",
    difficulty: "hard",
    domain: "travel",
  },
  {
    taskId: "m2w-hard-005",
    instruction:
      "Apply for a software engineering position, filling out the entire " +
      "application form including uploading a resume and cover letter",
    website: "greenhouse.io",
    difficulty: "hard",
    domain: "jobs",
  },
];
